#include "parser/parser.h"
#include <sstream>
#include <algorithm>
#include <iostream>

static std::string strip_delimiters(std::string s) {
    size_t s_idx = s.find_first_not_of(" \t\n\r");
    if (s_idx == std::string::npos) return "";
    size_t e_idx = s.find_last_not_of(" \t\n\r");
    s = s.substr(s_idx, e_idx - s_idx + 1);

    size_t dot_pos = s.find_last_of('.');
    if (dot_pos != std::string::npos) s = s.substr(dot_pos + 1);
    
    size_t p = s.find_first_of("(,; ");
    if (p != std::string::npos) s = s.substr(0, p);
    
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

Statement Parser::parse(const std::string& sql) {
    Statement stmt;
    stmt.type = StatementType::NONE;
    std::string clean_sql = sql;
    while (!clean_sql.empty() && (clean_sql.back() == ';' || std::isspace(clean_sql.back()))) clean_sql.pop_back();
    
    std::istringstream iss(clean_sql);
    std::string word;
    if (!(iss >> word)) return stmt;
    std::transform(word.begin(), word.end(), word.begin(), ::toupper);

    if (word == "CREATE") {
        stmt.type = StatementType::CREATE;
        iss >> word; // TABLE
        iss >> word; // IF or TABLE_NAME
        std::string upper_word = word;
        std::transform(upper_word.begin(), upper_word.end(), upper_word.begin(), ::toupper);
        if (upper_word == "IF") {
            iss >> word; // NOT
            iss >> word; // EXISTS
            iss >> word; // TABLE_NAME
        }
        stmt.create_stmt.table_name = strip_delimiters(word);
        size_t start_p = clean_sql.find('(');
        size_t end_p = clean_sql.find_last_of(')');
        if (start_p != std::string::npos && end_p != std::string::npos) {
            std::string cols = clean_sql.substr(start_p + 1, end_p - start_p - 1);
            std::stringstream ss(cols);
            std::string col_def;
            while (std::getline(ss, col_def, ',')) {
                std::stringstream css(col_def);
                std::string col_name, type;
                if (css >> col_name >> type) {
                    stmt.create_stmt.columns.push_back({strip_delimiters(col_name), strip_delimiters(type)});
                }
            }
        }
    } else if (word == "INSERT") {
        stmt.type = StatementType::INSERT;
        iss >> word; // INTO
        iss >> word; // table
        stmt.insert_stmt.table_name = strip_delimiters(word);
        size_t v_pos = clean_sql.find("VALUES");
        if (v_pos != std::string::npos) {
            stmt.insert_stmt.raw_values = clean_sql.substr(v_pos + 6);
        }
    } else if (word == "DELETE") {
        stmt.type = StatementType::DELETE;
        iss >> word; // FROM
        iss >> word; // table
        stmt.delete_stmt.table_name = strip_delimiters(word);
    } else if (word == "SELECT") {
        stmt.type = StatementType::SELECT;
        std::string line;
        std::getline(iss, line);
        size_t from_pos = line.find(" FROM ");
        if (from_pos != std::string::npos) {
            std::string cols = line.substr(0, from_pos);
            std::stringstream css(cols);
            std::string c;
            while (std::getline(css, c, ',')) {
                stmt.select_stmt.columns.push_back(strip_delimiters(c));
            }
            std::string rest = line.substr(from_pos + 6);
            std::stringstream rss(rest);
            std::string tbl_word;
            if (rss >> tbl_word) {
                stmt.select_stmt.table_name = strip_delimiters(tbl_word);
            }
            
            size_t join_pos = rest.find(" INNER JOIN ");
            if (join_pos != std::string::npos) {
                stmt.select_stmt.has_join = true;
                std::string join_part = rest.substr(join_pos + 12);
                size_t on_pos = join_part.find(" ON ");
                if (on_pos != std::string::npos) {
                    std::string join_tbl = join_part.substr(0, on_pos);
                    stmt.select_stmt.join.table = strip_delimiters(join_tbl);
                    std::string cond_part = join_part.substr(on_pos + 4);
                    size_t where_pos = cond_part.find(" WHERE ");
                    if (where_pos == std::string::npos) where_pos = cond_part.find(" ORDER BY ");
                    std::string on_cond = (where_pos != std::string::npos) ? cond_part.substr(0, where_pos) : cond_part;
                    size_t eq_pos = on_cond.find('=');
                    if (eq_pos != std::string::npos) {
                        stmt.select_stmt.join.on_left = strip_delimiters(on_cond.substr(0, eq_pos));
                        stmt.select_stmt.join.on_right = strip_delimiters(on_cond.substr(eq_pos + 1));
                    }
                }
            }
            
            size_t where_pos = rest.find(" WHERE ");
            if (where_pos != std::string::npos) {
                std::string where_clause = rest.substr(where_pos + 7);
                size_t order_pos = where_clause.find(" ORDER BY ");
                if (order_pos != std::string::npos) where_clause = where_clause.substr(0, order_pos);
                std::stringstream wss(where_clause);
                std::string col, op, val;
                while (wss >> col >> op >> val) {
                    if (strip_delimiters(col) == "AND") continue;
                    stmt.select_stmt.where.push_back({strip_delimiters(col), op, val});
                    std::string next_word; if (!(wss >> next_word)) break;
                }
            }
            
            size_t order_pos = rest.find(" ORDER BY ");
            if (order_pos != std::string::npos) {
                stmt.select_stmt.has_order = true;
                std::stringstream oss(rest.substr(order_pos + 10));
                std::string order_word;
                if (oss >> order_word) {
                    stmt.select_stmt.order_by.column = strip_delimiters(order_word);
                    std::string dir; if (oss >> dir) {
                        stmt.select_stmt.order_by.ascending = (strip_delimiters(dir) != "DESC");
                    }
                }
            }
        }
    }
        // --- APPENDIX: Final Validation Check ---
    bool is_valid = true;
    if (stmt.type == StatementType::CREATE) {
        if (stmt.create_stmt.table_name.empty() || stmt.create_stmt.columns.empty()) is_valid = false;
    } else if (stmt.type == StatementType::INSERT) {
        if (stmt.insert_stmt.table_name.empty() || stmt.insert_stmt.raw_values.empty()) is_valid = false;
    } else if (stmt.type == StatementType::SELECT) {
        if (stmt.select_stmt.table_name.empty() || stmt.select_stmt.columns.empty()) is_valid = false;
    } else if (stmt.type == StatementType::DELETE) {
        if (stmt.delete_stmt.table_name.empty()) is_valid = false;
    } else if (stmt.type == StatementType::NONE && !clean_sql.empty()) {
        // If it's NONE but there is actual text input, it's definitely an invalid command
        is_valid = false;
    }

    if (!is_valid) {
        stmt.type = StatementType::NONE;
    }
    // --- END APPENDIX ---
    return stmt;
}