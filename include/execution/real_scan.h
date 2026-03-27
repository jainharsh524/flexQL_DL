#pragma once

#include "storage/table.h"
#include "storage/column_reader.h"

#include <vector>

class RealScanner {
public:
    static std::vector<uint64_t> scan_all(
        ColumnReader& reader
    );
};