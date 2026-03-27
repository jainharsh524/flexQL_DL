#pragma once
#include "storage/table.h"
#include <string>
#include <vector>
#include <unordered_map>

class HashJoin {
public:
    std::vector<Row> join(Table* t1, Table* t2, const std::string& col1, const std::string& col2);
};
