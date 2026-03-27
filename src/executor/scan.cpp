#include "execution/scan.h"

std::vector<uint64_t> TableScanner::full_scan(Table* table) {
    std::vector<uint64_t> rows;

    // placeholder sequential scan
    for (uint64_t i = 0; i < 1000000; i++) {
        rows.push_back(i);
    }

    return rows;
}