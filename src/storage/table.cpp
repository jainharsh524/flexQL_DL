#include "storage/table.h"
#include <cstring>
#include <iostream>

Table::Table(const std::string& data_dir, const std::string& name, const Schema& schema)
    : name_(name), schema_(schema) {
    for (size_t i = 0; i < schema_.columns.size(); i++) {
        std::string path = data_dir + "/" + name_ + "_" + schema_.columns[i].name + ".bin";
        columns_.push_back(std::make_unique<ColumnStore>(path));
    }
    index_manager_ = std::make_unique<IndexManager>();
    if (std::string(schema_.columns[0].name) == "ID") {
        std::string pk_path = data_dir + "/" + name_ + "_PK.idx";
        index_manager_->create_index("PK", pk_path);
    }
}

void Table::insert_row(const Row& row) {
    uint64_t rid = columns_[0]->get_total_rows(); // Correct, high-speed RID
    for (size_t i = 0; i < schema_.columns.size() && i < row.values.size(); i++) {
        uint8_t buf[128] = {0};
        const auto& v = row.values[i];
        uint16_t sz = (v.type == ColumnType::VARCHAR) ? 128 : (v.type == ColumnType::INT || v.type == ColumnType::FLOAT ? 4 : 8);
        if (v.type == ColumnType::VARCHAR) {
            strncpy((char*)buf, v.str.c_str(), 127);
        } else if (v.type == ColumnType::INT) {
            memcpy(buf, &v.i32, 4);
        } else if (v.type == ColumnType::FLOAT) {
            memcpy(buf, &v.f32, 4);
        } else {
            memcpy(buf, &v.i64, 8);
        }
        columns_[i]->insert_fixed(buf, sz);
    }
    if (std::string(schema_.columns[0].name) == "ID") {
        index_manager_->get_index("PK")->insert(row.values[0].i64, rid);
    }
}

void Table::clear() {
    for (auto& col : columns_) col->clear();
}

void Table::flush_all() {
    for (auto& col : columns_) col->flush();
}

ColumnStore* Table::get_column(size_t index) {
    if (index >= columns_.size()) return nullptr;
    return columns_[index].get();
}

void Table::scan(const std::function<bool(const Row&, RID)>& cb) {
    (void)cb; 
}

bool Table::fetch_row(RID rid, Row& out) {
    (void)rid; (void)out; return false; 
}