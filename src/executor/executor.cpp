#include "executor/executor.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <sstream>
#include "storage/column_reader.h"
#include "executor/hash_join.h"

// Note: Value and Row are defined in storage/row.h, which should be included via storage/table.h

Executor::Executor(const std::string& data_dir) : data_dir_(data_dir) {}

int Executor::exec(const Statement& s, ExecCallback cb, void* arg, std::string& err) {
    if (s.type == StatementType::CREATE) return exec_create(s.create_stmt, err);
    if (s.type == StatementType::INSERT) return exec_insert(s.insert_stmt, err);
    if (s.type == StatementType::SELECT) return exec_select(s.select_stmt, cb, arg, err);
    if (s.type == StatementType::DELETE) return exec_delete(s.delete_stmt, err);
    return 1;
}

Table* Executor::get_table(const std::string& name, std::string& err) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), ::toupper);
    std::unique_lock<std::mutex> lk(tables_mu_);
    if (!tables_.count(n)) { err = "Table not found: " + n; return nullptr; }
    return tables_[n].get();
}

int Executor::exec_create(const CreateStmt& s, std::string& err) {
    if (s.table_name.empty()) { err = "Missing table name"; return 1; }
    std::unique_lock<std::mutex> lk(tables_mu_);
    if (tables_.count(s.table_name)) return 0; // IF NOT EXISTS
    Schema schema;
    for (const auto& col : s.columns) {
        ColumnDef d; strncpy(d.name, col.name.c_str(), 31);
        if (col.type_str == "DECIMAL" || col.type_str == "INT") d.type = ColumnType::DECIMAL;
        else if (col.type_str == "FLOAT") d.type = ColumnType::FLOAT;
        else d.type = ColumnType::VARCHAR;
        schema.columns.push_back(d);
    }
    tables_[s.table_name] = std::make_unique<Table>(data_dir_, s.table_name, schema);
    return 0;
}

int Executor::exec_insert(const InsertStmt& s, std::string& err) {
    Table* t = get_table(s.table_name, err);
    if (!t) return 1;
    const std::string& raw = s.raw_values;
    size_t pos = 0;
    while ((pos = raw.find('(', pos)) != std::string::npos) {
        size_t end = raw.find(')', pos);
        if (end == std::string::npos) break;
        std::string tuple = raw.substr(pos + 1, end - pos - 1);
        pos = end + 1;
        Row row;
        std::stringstream ss(tuple);
        std::string val;
        int col_idx = 0;
        while (std::getline(ss, val, ',')) {
            while(!val.empty() && std::isspace(val[0])) val.erase(0,1);
            while(!val.empty() && std::isspace(val.back())) val.pop_back();
            Value cv;
            auto col_type = t->schema().columns[col_idx].type;
            if (col_type == ColumnType::VARCHAR) {
                if (val.size() >= 2 && val.front() == '\'' && val.back() == '\'') val = val.substr(1, val.size()-2);
                cv.type = ColumnType::VARCHAR; cv.str = val;
            } else if (col_type == ColumnType::DECIMAL || col_type == ColumnType::INT) {
                cv.type = ColumnType::DECIMAL; cv.i64 = std::stoll(val);
            } else {
                cv.type = ColumnType::FLOAT; cv.f32 = std::stof(val);
            }
            row.values.push_back(cv);
            col_idx++;
        }
        t->insert_row(row);
    }
    return 0;
}

Row Executor::materialize_row(Table* table, RID rid) {
    Row row;
    const auto& schema = table->schema();
    for (size_t c = 0; c < schema.columns.size(); c++) {
        ColumnReader r(table->get_column(c)->get_pager(), table->get_column(c)->get_cache());
        std::vector<uint8_t> buf;
        Value v; v.type = schema.columns[c].type;
        uint16_t fsz = (v.type == ColumnType::VARCHAR) ? 128 : 8;
        if (r.read_at(rid, buf, fsz)) {
            if (v.type == ColumnType::VARCHAR) {
                buf.push_back(0); // Ensure null termination
                v.str = std::string((char*)buf.data());
            } else memcpy(&v.i64, buf.data(), 8);
        }
        row.values.push_back(v);
    }
    return row;
}

int Executor::exec_select(const SelectStmt& s, ExecCallback cb, void* arg, std::string& err) {
    Table* t = get_table(s.table_name, err);
    if (!t) return 1;
    // --- Validate requested column names against schema ---
    if (!s.has_join) { // For joins, columns from both tables, validated differently
        for (const auto& col_name : s.columns) {
            bool found = false;
            for (const auto& def : t->schema().columns) {
                if (std::string(def.name) == col_name) { found = true; break; }
            }
            if (!found && col_name != "*") {
                err = "Unknown column: " + col_name;
                return 1;
            }
        }
    }
// --- End validation ---

    std::vector<Row> results;
    bool used_index = false;
    Table* t2 = nullptr;

    if (!s.has_join && s.where.size() == 1 && s.where[0].column_name == "ID" && s.where[0].op == "=") {
        try {
            uint64_t id = std::stoll(s.where[0].value_str);
            RID rid;
            if (t->get_index()->get_index("PK")->search(id, rid)) {
                results.push_back(materialize_row(t, rid));
                used_index = true;
            }
        } catch(...) {}
    }

    if (!used_index) {
        if (s.has_join) {
            t2 = get_table(s.join.table, err);
            if (!t2) return 1;
            HashJoin hj;
            results = hj.join(t, t2, s.join.on_left, s.join.on_right);
        } else {
            uint64_t total = t->get_column(0)->get_total_rows();
            for (RID rid = 0; rid < total; rid++) {
                results.push_back(materialize_row(t, rid));
            }
        }
    }

    // Filter
    if (!used_index) {
        auto it = results.begin();
        while (it != results.end()) {
            bool match = true;
            for (const auto& w : s.where) {
                int col_idx = -1;
                for (size_t i = 0; i < t->schema().columns.size(); i++) {
                    if (std::string(t->schema().columns[i].name) == w.column_name) { col_idx = i; break; }
                }
                if (col_idx == -1 && t2) {
                    for (size_t i = 0; i < t2->schema().columns.size(); i++) {
                        if (std::string(t2->schema().columns[i].name) == w.column_name) { 
                            col_idx = (int)(t->schema().columns.size() + i); break; 
                        }
                    }
                }
                if (col_idx == -1) continue;
                const auto& val = it->values[col_idx];
                if (w.op == "=") {
                    if (val.type == ColumnType::VARCHAR) match = (val.str == w.value_str);
                    else match = (val.i64 == std::stoll(w.value_str));
                } else if (w.op == ">") { match = (val.i64 > std::stoll(w.value_str)); }
                else if (w.op == ">=") { match = (val.i64 >= std::stoll(w.value_str)); }
                if (!match) break;
            }
            if (!match) it = results.erase(it); else ++it;
        }
    }

    if (s.has_order) {
        int col_idx = -1;
        for (size_t i = 0; i < t->schema().columns.size(); i++) {
            if (std::string(t->schema().columns[i].name) == s.order_by.column) { col_idx = i; break; }
        }
        if (col_idx == -1 && t2) {
             for (size_t i = 0; i < t2->schema().columns.size(); i++) {
                if (std::string(t2->schema().columns[i].name) == s.order_by.column) { col_idx = (int)(t->schema().columns.size() + i); break; }
            }
        }
        if (col_idx != -1) {
            std::sort(results.begin(), results.end(), [&](const Row& a, const Row& b) {
                const auto& va = a.values[col_idx]; const auto& vb = b.values[col_idx];
                bool lt = (va.type == ColumnType::VARCHAR) ? (va.str < vb.str) : (va.i64 < vb.i64);
                return s.order_by.ascending ? lt : !lt;
            });
        }
    }

    // Final Callback with STABLE string pointers
    for (const auto& r : results) {
        std::vector<char*> argv;
        std::vector<std::string> strings;
        strings.reserve(s.columns.size()); // CRITICAL: Avoid pointer invalidation
        
        for (const auto& col_name : s.columns) {
            int c_idx = -1;
            for (size_t j = 0; j < t->schema().columns.size(); j++) {
                if (std::string(t->schema().columns[j].name) == col_name) { c_idx = (int)j; break; }
            }
            if (c_idx == -1 && t2) {
                for (size_t j = 0; j < t2->schema().columns.size(); j++) {
                    if (std::string(t2->schema().columns[j].name) == col_name) { 
                        c_idx = (int)(t->schema().columns.size() + j); break; 
                    }
                }
            }
            if (c_idx != -1 && (size_t)c_idx < r.values.size()) {
                if (r.values[c_idx].type == ColumnType::VARCHAR) 
                    strings.push_back(r.values[c_idx].str);
                else 
                    strings.push_back(std::to_string(r.values[c_idx].i64));
            } else {
                strings.push_back("NULL");
            }
        }
        // Second pass once all strings are pushed and vector is stable
        for (auto& st : strings) argv.push_back((char*)st.c_str());
        
        cb(arg, (int)argv.size(), argv.data(), nullptr);
    }
    return 0;
}

int Executor::exec_delete(const DeleteStmt& s, std::string& err) {
    Table* t = get_table(s.table_name, err);
    if (!t) return 1;
    t->clear();
    return 0;
}