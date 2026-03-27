#pragma once

#include "table.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class TableManager {
private:
    std::unordered_map<std::string, std::unique_ptr<Table>> tables;
    std::mutex manager_mutex;

public:
    void create_table(const TableSchema& schema, const std::string& base_path);

    Table* get_table(const std::string& name);
};