#include "cache/lru_cache.h"

LRUCache::LRUCache(size_t capacity, Pager& pager)
    : capacity(capacity), pager(pager) {}

LRUCache::~LRUCache() {
    clear();
}

Page* LRUCache::get(uint32_t page_id) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    if (map.find(page_id) == map.end()) return nullptr;
    auto it = map[page_id];
    lru_list.splice(lru_list.begin(), lru_list, it);
    return it->page;
}

void LRUCache::put(uint32_t page_id, Page* page, bool dirty) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    if (map.find(page_id) != map.end()) {
        auto it = map[page_id];
        it->page = page;
        it->dirty = dirty;
        lru_list.splice(lru_list.begin(), lru_list, it);
        return;
    }
    if (lru_list.size() >= capacity) {
        evict_internal();
    }
    lru_list.push_front({page_id, page, dirty});
    map[page_id] = lru_list.begin();
}

bool LRUCache::exists(uint32_t page_id) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    return map.find(page_id) != map.end();
}

void LRUCache::flush_all() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    flush_all_internal();
}

void LRUCache::flush_all_internal() {
    for (auto& entry : lru_list) {
        if (entry.dirty) {
            pager.flush(entry.page_id);
            entry.dirty = false;
        }
    }
}

void LRUCache::clear() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    flush_all_internal();
    // 🔥 REMOVED delete entry.page; (backed by mmap)
    lru_list.clear();
    map.clear();
}

void LRUCache::evict_internal() {
    auto last = lru_list.back();
    if (last.dirty) {
        pager.flush(last.page_id);
    }
    // 🔥 REMOVED delete last.page; (backed by mmap)
    lru_list.pop_back();
    map.erase(last.page_id);
}

void LRUCache::evict() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    evict_internal();
}