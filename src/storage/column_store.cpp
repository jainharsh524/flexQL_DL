#include "storage/column_store.h"
#include <cstring>

ColumnStore::ColumnStore(const std::string& file_path) {
    pager_ = std::make_unique<Pager>(file_path);
    cache_ = std::make_unique<CacheManager>(*pager_, 256);
    
    // Compute total rows on startup
    total_rows_ = 0;
    uint32_t np = pager_->get_num_pages();
    for (uint32_t i = 0; i < np; i++) {
        Page* p = cache_->get_page(i);
        ColumnPageHeader* h = (ColumnPageHeader*)p->data;
        total_rows_ += h->num_items;
    }
}

ColumnStore::~ColumnStore() = default;

void ColumnStore::insert_fixed(const void* data, uint16_t item_size) {
    if (!active_page_ || active_header_->num_items >= (4096 - sizeof(ColumnPageHeader)) / item_size) {
        uint32_t last = pager_->get_num_pages();
        if (last > 0) {
            active_page_ = cache_->get_page(last - 1);
            active_header_ = (ColumnPageHeader*)active_page_->data;
            if (active_header_->num_items >= (4096 - sizeof(ColumnPageHeader)) / item_size) {
                active_page_idx_ = pager_->allocate_page();
                active_page_ = cache_->get_page(active_page_idx_);
                if (!active_page_) return;
                memset(active_page_->data, 0, 4096);
                active_header_ = (ColumnPageHeader*)active_page_->data;
                active_header_->item_size = item_size;
            } else {
                active_page_idx_ = last - 1;
            }
        } else {
            active_page_idx_ = pager_->allocate_page();
            active_page_ = cache_->get_page(active_page_idx_);
            memset(active_page_->data, 0, 4096);
            active_header_ = (ColumnPageHeader*)active_page_->data;
            active_header_->item_size = item_size;
        }
    }

    memcpy(active_page_->data + sizeof(ColumnPageHeader) + active_header_->num_items * item_size, data, item_size);
    active_header_->num_items++;
    total_rows_++;
    cache_->mark_dirty(active_page_idx_);
}

void ColumnStore::insert_variable(const uint8_t* data, uint16_t size) {
    uint32_t slot_size = 128;
    if (!active_page_ || active_header_->item_size != 0 || active_header_->num_items >= (4096 - sizeof(ColumnPageHeader)) / slot_size) {
        uint32_t last = pager_->get_num_pages();
        if (last > 0) {
            active_page_ = cache_->get_page(last - 1);
            active_header_ = (ColumnPageHeader*)active_page_->data;
            if (active_header_->item_size != 0 || active_header_->num_items >= (4096 - sizeof(ColumnPageHeader)) / slot_size) {
                active_page_idx_ = pager_->allocate_page();
                active_page_ = cache_->get_page(active_page_idx_);
                if (!active_page_) return;
                memset(active_page_->data, 0, 4096);
                active_header_ = (ColumnPageHeader*)active_page_->data;
                active_header_->item_size = 0;
            } else {
                active_page_idx_ = last - 1;
            }
        } else {
            active_page_idx_ = pager_->allocate_page();
            active_page_ = cache_->get_page(active_page_idx_);
            memset(active_page_->data, 0, 4096);
            active_header_ = (ColumnPageHeader*)active_page_->data;
            active_header_->item_size = 0;
        }
    }

    uint8_t* slot = active_page_->data + sizeof(ColumnPageHeader) + active_header_->num_items * slot_size;
    memcpy(slot, &size, 2);
    memcpy(slot + 2, data, (size > 126 ? 126 : size));
    active_header_->num_items++;
    total_rows_++;
    cache_->mark_dirty(active_page_idx_);
}

void ColumnStore::flush() {
    cache_->flush();
}

void ColumnStore::clear() {
    active_page_ = nullptr;
    active_header_ = nullptr;
    active_page_idx_ = 0xFFFFFFFF;
    total_rows_ = 0;
    cache_->clear();
    pager_->clear();
}

Pager& ColumnStore::get_pager() { return *pager_; }
CacheManager& ColumnStore::get_cache() { return *cache_; }