#pragma once

#include "storage/table.h"
#include <vector>

class TableScanner {
public:
    static std::vector<uint64_t> full_scan(Table* table);
};