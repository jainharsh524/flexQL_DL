#pragma once

#include "page.h"
#include <string>
#include <cstdint>

class Pager {
private:
    int fd;
    uint8_t* file_data;

    uint32_t num_pages;
    size_t file_size;

public:
    Pager(const std::string& filename);
    ~Pager();

    Page* get_page(uint32_t page_id);

    uint32_t allocate_page();

    void flush(uint32_t page_id);
    void flush_all();

    void clear();

    uint32_t get_num_pages() const;
};