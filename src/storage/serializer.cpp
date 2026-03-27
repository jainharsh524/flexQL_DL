#include "storage/serializer.h"
#include <cstring>
#include <iostream>

int32_t Serializer::encode_int(const std::string& val) {
    try { return std::stoi(val); } catch (...) { return 0; }
}

int64_t Serializer::encode_datetime(const std::string& val) {
    try { return std::stoll(val); } catch (...) { return 0; }
}

std::vector<uint8_t> Serializer::encode_varchar(const std::string& val) {
    return std::vector<uint8_t>(val.begin(), val.end());
}

void Serializer::serialize_row(
    const Schema& schema,
    const Row& row,
    std::vector<std::vector<uint8_t>>& column_data
) {
    size_t n = schema.columns.size();
    column_data.resize(n + 1);

    for (size_t i = 0; i < n; i++) {
        if (i >= row.values.size()) continue;
        const Value& v = row.values[i];
        if (v.type == ColumnType::INT) {
            column_data[i].resize(sizeof(int32_t));
            memcpy(column_data[i].data(), &v.i32, sizeof(int32_t));
        } else if (v.type == ColumnType::DATETIME || v.type == ColumnType::DECIMAL) {
            column_data[i].resize(sizeof(int64_t));
            memcpy(column_data[i].data(), &v.i64, sizeof(int64_t));
        } else if (v.type == ColumnType::VARCHAR || v.type == ColumnType::CHAR) {
            column_data[i] = encode_varchar(v.str);
        } else if (v.type == ColumnType::FLOAT) {
            column_data[i].resize(sizeof(float));
            memcpy(column_data[i].data(), &v.f32, sizeof(float));
        }
    }

    // Expiry
    int64_t expiry = now_ts() + 315360000;
    if (schema.expiry_col_idx < n && schema.expiry_col_idx < row.values.size())
        expiry = row.values[schema.expiry_col_idx].i64;
    column_data[n].resize(sizeof(int64_t));
    memcpy(column_data[n].data(), &expiry, sizeof(int64_t));
}