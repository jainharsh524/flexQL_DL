#pragma once
#include "lru_cache.h"
#include "storage/pager.h"

class CacheManager {
private:
    LRUCache cache;
    Pager& pager;

public:
    CacheManager(Pager& pager, size_t capacity);
    Page* get_page(uint32_t page_id);
    void mark_dirty(uint32_t page_id);
    
    void flush();
    void clear();
};