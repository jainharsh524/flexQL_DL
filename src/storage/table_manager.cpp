#include "storage/table_manager.h"

void TableManager::create_table(const TableSchema& schema, const std::string& base_path) {
    std::lock_guard<std::mutex> lock(manager_mutex);

    tables[schema.table_name] =
        std::make_unique<Table>(schema, base_path);
}

Table* TableManager::get_table(const std::string& name) {
    std::lock_guard<std::mutex> lock(manager_mutex);

    if (tables.find(name) == tables.end()) {
        return nullptr;
    }

    return tables[name].get();
}