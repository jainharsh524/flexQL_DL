#include "storage/batch_buffer.h"

ColumnBatchBuffer::ColumnBatchBuffer(size_t batch_size)
    : max_batch_size(batch_size) {}

void ColumnBatchBuffer::add(const std::vector<uint8_t>& value) {
    buffer.push_back(value);
}

bool ColumnBatchBuffer::is_full() const {
    return buffer.size() >= max_batch_size;
}

const std::vector<std::vector<uint8_t>>& ColumnBatchBuffer::get_data() const {
    return buffer;
}

void ColumnBatchBuffer::clear() {
    buffer.clear();
}