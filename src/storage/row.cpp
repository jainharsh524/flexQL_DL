#include "storage/row.h"
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <algorithm>

void Schema::finalize() {
    col_offsets.resize(columns.size());
    uint32_t off = 0;
    for (size_t i = 0; i < columns.size(); ++i) {
        col_offsets[i] = static_cast<uint16_t>(off);
        const ColumnDef& c = columns[i];
        switch (c.type) {
            case ColumnType::INT:      off += 4; break;
            case ColumnType::FLOAT:    off += 4; break;
            case ColumnType::DECIMAL:  off += 8; break;
            case ColumnType::CHAR:     off += c.length; break;
            case ColumnType::VARCHAR:  off += 2 + c.length; break;
            case ColumnType::DATETIME: off += 8; break;
        }
    }
    fixed_row_size = off;
}

bool Value::operator==(const Value& o) const {
    if (type != o.type) return false;
    switch (type) {
        case ColumnType::INT:      return i32 == o.i32;
        case ColumnType::FLOAT:    return f32 == o.f32;
        case ColumnType::DECIMAL:
        case ColumnType::DATETIME: return i64 == o.i64;
        case ColumnType::CHAR:
        case ColumnType::VARCHAR:  return str == o.str;
    }
    return false;
}

bool Value::operator<(const Value& o) const {
    switch (type) {
        case ColumnType::INT:      return i32 < o.i32;
        case ColumnType::FLOAT:    return f32 < o.f32;
        case ColumnType::DECIMAL:
        case ColumnType::DATETIME: return i64 < o.i64;
        case ColumnType::CHAR:
        case ColumnType::VARCHAR:  return str < o.str;
    }
    return false;
}

bool Value::operator>(const Value& o) const { return o < *this; }

std::string Value::to_string() const {
    switch (type) {
        case ColumnType::INT:      return std::to_string(i32);
        case ColumnType::FLOAT:    return std::to_string(f32);
        case ColumnType::DECIMAL:  return std::to_string(i64); // simplified
        case ColumnType::DATETIME: return format_datetime(i64);
        case ColumnType::CHAR:
        case ColumnType::VARCHAR:  return str;
    }
    return "";
}

uint16_t Row::serialize(const Schema&, uint8_t*, uint16_t) const { return 0; }
bool Row::deserialize(const Schema&, const uint8_t*, uint16_t) { return false; }
bool Row::is_expired(const Schema& schema) const {
    if (schema.expiry_col_idx >= values.size()) return false;
    int64_t expiry = values[schema.expiry_col_idx].i64;
    return expiry != 0 && expiry < now_ts();
}

int64_t parse_datetime(const std::string& s) {
    struct tm t{};
    if (sscanf(s.c_str(), "%d-%d-%d %d:%d:%d",
               &t.tm_year, &t.tm_mon, &t.tm_mday,
               &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
        return 0;
    t.tm_year -= 1900;
    t.tm_mon  -= 1;
    t.tm_isdst = -1;
    return static_cast<int64_t>(mktime(&t));
}

std::string format_datetime(int64_t ts) {
    time_t t = static_cast<time_t>(ts);
    struct tm* tm_info = localtime(&t);
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return buf;
}
