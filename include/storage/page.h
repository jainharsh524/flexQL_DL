#pragma once
#include <cstdint>

constexpr uint32_t PAGE_SIZE = 4096;
using RID = uint64_t;

// Columnar page header
struct PageHeader {
    uint16_t num_items;         // number of values
    uint16_t free_space_offset; // for variable size types
};

// Generic page
class Page {
public:
    uint8_t data[PAGE_SIZE];

    inline PageHeader* header() {
        return reinterpret_cast<PageHeader*>(data);
    }

    inline uint8_t* body() {
        return data + sizeof(PageHeader);
    }
};