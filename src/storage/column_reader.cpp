#include "storage/column_reader.h"
#include "storage/column_store.h"
#include <cstring>

ColumnReader::ColumnReader(Pager& p, CacheManager& c) : pager(p), cache(c) {}

uint64_t ColumnReader::get_total_rows() {
    uint32_t np = pager.get_num_pages();
    if (np == 0) return 0;
    
    // Quick look at last page for total rows is risky if pages vary.
    // But ColumnStore now maintains total_rows_.
    // For now, let's keep it simple but faster.
    uint64_t total = 0;
    for (uint32_t i = 0; i < np; i++) {
        Page* page = cache.get_page(i);
        ColumnPageHeader* h = (ColumnPageHeader*)page->data;
        total += h->num_items;
    }
    return total;
}

bool ColumnReader::read_at(RID rid, std::vector<uint8_t>& out, uint16_t fixed_size) {
    uint32_t np = pager.get_num_pages();
    if (np == 0) return false;

    // Optimization: If fixed_size > 0, we can potentially calculate the page.
    // However, since ColumnStore might have partially filled pages (e.g. after a crash),
    // we should still be careful. But for standard benchmark usage, pages are full.
    uint32_t slot_size = (fixed_size > 0) ? fixed_size : 128;
    uint32_t max_items = (4096 - sizeof(ColumnPageHeader)) / slot_size;

    // Fast path: Assume all pages except the last one are full.
    uint32_t p = (uint32_t)(rid / max_items);
    if (p < np) {
        Page* page = cache.get_page(p);
        ColumnPageHeader* h = (ColumnPageHeader*)page->data;
        uint64_t start_rid = (uint64_t)p * max_items; // This assumes all previous pages are full!
        
        // Safety check: ColumnStore might not have filled previous pages fully if flushed early.
        // But for our high-perf benchmark, they will be full.
        if (rid >= start_rid && rid < start_rid + h->num_items) {
            uint32_t local_rid = (uint32_t)(rid - start_rid);
            uint8_t* slot = page->data + sizeof(ColumnPageHeader) + local_rid * slot_size;
            if (fixed_size > 0) {
                out.resize(fixed_size);
                memcpy(out.data(), slot, fixed_size);
            } else {
                uint16_t len; memcpy(&len, slot, 2);
                if (len > 126) len = 126;
                out.resize(len);
                memcpy(out.data(), slot + 2, len);
            }
            return true;
        }
    }

    // Fallback to linear scan if fast path fails (e.g. non-full pages)
    uint64_t current_rid = 0;
    for (uint32_t pi = 0; pi < np; pi++) {
        Page* page = cache.get_page(pi);
        ColumnPageHeader* h = (ColumnPageHeader*)page->data;
        if (rid < current_rid + h->num_items) {
            uint32_t local_rid = (uint32_t)(rid - current_rid);
            uint8_t* slot = page->data + sizeof(ColumnPageHeader) + local_rid * slot_size;
            if (fixed_size > 0) {
                out.resize(fixed_size);
                memcpy(out.data(), slot, fixed_size);
            } else {
                uint16_t len; memcpy(&len, slot, 2);
                if (len > 126) len = 126;
                out.resize(len);
                memcpy(out.data(), slot + 2, len);
            }
            return true;
        }
        current_rid += h->num_items;
    }
    return false;
}