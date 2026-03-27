#pragma once

#include <vector>
#include <cstdint>

// Batch insert buffer (reduces disk I/O)
class ColumnBatchBuffer {
private:
    std::vector<std::vector<uint8_t>> buffer;
    size_t max_batch_size;

public:
    ColumnBatchBuffer(size_t batch_size = 1024);

    void add(const std::vector<uint8_t>& value);

    bool is_full() const;

    const std::vector<std::vector<uint8_t>>& get_data() const;

    void clear();
};