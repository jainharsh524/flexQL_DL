#pragma once
#include "storage/page.h"
#include "storage/pager.h"
#include <unordered_map>
#include <list>
#include <mutex>

struct CacheEntry {
    uint32_t page_id;
    Page* page;
    bool dirty;
};

class LRUCache {
private:
    size_t capacity;
    std::list<CacheEntry> lru_list;
    std::unordered_map<uint32_t, std::list<CacheEntry>::iterator> map;
    mutable std::mutex cache_mutex;
    Pager& pager;

public:
    LRUCache(size_t capacity, Pager& pager);
    ~LRUCache();

    Page* get(uint32_t page_id);
    void put(uint32_t page_id, Page* page, bool dirty);
    bool exists(uint32_t page_id);
    
    void flush_all();
    void clear();
    void evict();

private:
    void flush_all_internal();
    void evict_internal();
};