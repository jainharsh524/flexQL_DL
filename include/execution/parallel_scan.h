#pragma once

#include "storage/column_reader.h"
#include <vector>

class ParallelScanner {
public:
    static std::vector<uint64_t> scan(
        ColumnReader& reader,
        int num_threads
    );
};