#include "cache/cache_manager.h"

CacheManager::CacheManager(Pager& pager, size_t capacity)
    : cache(capacity, pager), pager(pager) {}

Page* CacheManager::get_page(uint32_t page_id) {
    Page* page = cache.get(page_id);
    if (page != nullptr) return page;
    page = pager.get_page(page_id);
    cache.put(page_id, page, false);
    return page;
}

void CacheManager::mark_dirty(uint32_t page_id) {
    Page* page = cache.get(page_id);
    if (page) {
        cache.put(page_id, page, true);
    }
}

void CacheManager::flush() {
    cache.flush_all();
}

void CacheManager::clear() {
    cache.clear();
}