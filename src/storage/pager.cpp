#include "storage/pager.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

// =======================
// Constructor
// =======================

Pager::Pager(const std::string& filename) {
    fd = open(filename.c_str(), O_RDWR | O_CREAT, 0644);

    if (fd < 0) {
        perror("open failed");
        exit(1);
    }

    // 🔥 Allocate large file upfront (1GB)
    size_t MAX_FILE_SIZE = 1ULL << 30; // 1GB

    if (ftruncate(fd, MAX_FILE_SIZE) != 0) {
        perror("ftruncate failed");
        exit(1);
    }

    file_size = MAX_FILE_SIZE;

    file_data = (uint8_t*) mmap(
        nullptr,
        file_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (file_data == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    num_pages = 0;
}

// =======================
// Destructor
// =======================

Pager::~Pager() {
    msync(file_data, file_size, MS_SYNC);
    munmap(file_data, file_size);
    close(fd);
}

// =======================
// Get Page
// =======================

Page* Pager::get_page(uint32_t page_id) {
    if (page_id >= num_pages) {
        std::cerr << "Invalid page access: " << page_id << std::endl;
        return nullptr;
    }

    return reinterpret_cast<Page*>(
        file_data + page_id * PAGE_SIZE
    );
}

// =======================
// Allocate Page (SAFE)
// =======================

uint32_t Pager::allocate_page() {
    uint32_t new_page_id = num_pages;

    num_pages++;

    // 🔥 IMPORTANT: NO mmap / munmap here
    // memory stays stable

    return new_page_id;
}

// =======================
// Flush
// =======================

void Pager::flush(uint32_t page_id) {
    msync(
        file_data + page_id * PAGE_SIZE,
        PAGE_SIZE,
        MS_SYNC
    );
}

void Pager::flush_all() {
    msync(file_data, file_size, MS_SYNC);
}

// =======================
// Page count
// =======================

void Pager::clear() {
    num_pages = 0;
}

uint32_t Pager::get_num_pages() const {
    return num_pages;
}