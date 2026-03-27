#include "storage/schema.h"

TableSchema::TableSchema(const std::string& name)
    : table_name(name), expiry_index(-1) {}

void TableSchema::add_column(const std::string& name, ColumnType type) {
    columns.push_back({name, type});
}

size_t TableSchema::column_count() const {
    return columns.size();
}