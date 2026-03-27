#pragma once

#include "parser/query.h"
#include "storage/table_manager.h"
#include "index/index_manager.h"

#include <vector>
#include <string>

class Executor {
private:
    TableManager& table_manager;
    IndexManager& index_manager;

public:
    Executor(TableManager& tm, IndexManager& im);

    std::vector<std::vector<std::string>> execute(const Query& query);

private:
    std::vector<uint64_t> scan_table(Table* table);
    std::vector<uint64_t> index_lookup(const std::string& index, uint64_t key);

    bool check_expiry(Table* table, uint64_t row_id);
};