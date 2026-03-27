#pragma once
#include "pager.h"
#include "cache/cache_manager.h"
#include <cstdint>
#include <vector>

class ColumnReader {
private:
    Pager& pager;
    CacheManager& cache;

public:
    ColumnReader(Pager& pager, CacheManager& cache);
    uint64_t get_total_rows();

    // Generic read. fixed_size > 0 for fixed columns, 0 for variable
    bool read_at(uint64_t row_id, std::vector<uint8_t>& out, uint16_t fixed_size = 0);

    int32_t read_int(uint64_t row_id);
    int64_t read_datetime(uint64_t row_id);
};