#include "execution/real_scan.h"

std::vector<uint64_t> RealScanner::scan_all(ColumnReader& reader) {
    std::vector<uint64_t> rows;

    uint64_t total = reader.get_total_rows();

    rows.reserve(total);

    for (uint64_t i = 0; i < total; i++) {
        rows.push_back(i);
    }

    return rows;
}