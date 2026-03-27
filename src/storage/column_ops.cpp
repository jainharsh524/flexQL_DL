#include "storage/column_ops.h"
#include <cstring>

bool insert_fixed(Page* page, const void* data, uint16_t size) {
    auto hdr = page->header();
    if (hdr->free_space_offset + size > PAGE_SIZE) return false;
    memcpy(page->data + hdr->free_space_offset, data, size);
    hdr->free_space_offset += size;
    hdr->num_items++;
    return true;
}

bool insert_variable(Page* page, const uint8_t* data, uint16_t size) {
    auto hdr = page->header();
    // Store 2-byte size + data
    if (hdr->free_space_offset + 2 + size > PAGE_SIZE) return false;
    memcpy(page->data + hdr->free_space_offset, &size, 2);
    memcpy(page->data + hdr->free_space_offset + 2, data, size);
    hdr->free_space_offset += (2 + size);
    hdr->num_items++;
    return true;
}