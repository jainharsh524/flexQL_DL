#pragma once
#include "storage/pager.h"
#include "cache/cache_manager.h"
#include <string>
#include <memory>
#include <cstdint>

struct ColumnPageHeader {
    uint32_t num_items;
    uint32_t item_size; // 0 for variable
};

class ColumnStore {
public:
    ColumnStore(const std::string& file_path);
    ~ColumnStore();

    void insert_fixed(const void* data, uint16_t size);
    void insert_variable(const uint8_t* data, uint16_t size);

    void flush();
    void clear();

    Pager& get_pager();
    CacheManager& get_cache();
    uint64_t get_total_rows() { return total_rows_; }

private:
    std::unique_ptr<Pager>        pager_;
    std::unique_ptr<CacheManager> cache_;
    
    uint64_t total_rows_ = 0;
    
    // Performance cache for insertion
    Page* active_page_ = nullptr;
    ColumnPageHeader* active_header_ = nullptr;
    uint32_t active_page_idx_ = 0xFFFFFFFF;
};