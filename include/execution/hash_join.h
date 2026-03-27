#pragma once

#include "storage/column_reader.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

class HashJoin {
public:
    static std::vector<std::pair<uint64_t, uint64_t>>
    execute(
        ColumnReader& left_reader,
        ColumnReader& right_reader,
        const std::vector<uint64_t>& left_rows,
        const std::vector<uint64_t>& right_rows
    );
};