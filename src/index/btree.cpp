#include "index/btree.h"
#include <cstring>
#include <iostream>

BPlusTree::BPlusTree(const std::string& filename)
    : pager_(filename),
      cache_manager_(pager_, 1024) // 4MB cache
{
    root_pid_ = pager_.allocate_page();
    Page* pg = cache_manager_.get_page(root_pid_);
    auto node = reinterpret_cast<BPTreeNode*>(pg->data);
    node->header.type = NodeType::LEAF;
    node->header.num_keys = 0;
    node->next_leaf = 0;
}

void BPlusTree::insert(uint64_t key, uint64_t row_id) {
    uint32_t leaf = find_leaf(key);
    insert_into_leaf(leaf, key, row_id);
    
    Page* pg = cache_manager_.get_page(leaf);
    auto node = reinterpret_cast<BPTreeNode*>(pg->data);
    if (node->header.num_keys >= BPTREE_ORDER) {
        split_leaf(leaf);
    }
    cache_manager_.mark_dirty(leaf);
}

bool BPlusTree::search(uint64_t key, uint64_t& result) {
    uint32_t leaf = find_leaf(key);
    Page* pg = cache_manager_.get_page(leaf);
    auto node = reinterpret_cast<BPTreeNode*>(pg->data);
    for (int i = 0; i < node->header.num_keys; i++) {
        if (node->keys[i] == key) {
            result = node->values[i];
            return true;
        }
    }
    return false;
}

uint32_t BPlusTree::find_leaf(uint64_t key) {
    uint32_t pid = root_pid_;
    while (true) {
        Page* pg = cache_manager_.get_page(pid);
        auto node = reinterpret_cast<BPTreeNode*>(pg->data);
        if (node->header.type == NodeType::LEAF) return pid;
        
        int i = 0;
        while (i < node->header.num_keys && key >= node->keys[i]) i++;
        pid = node->children[i];
    }
}

void BPlusTree::insert_into_leaf(uint32_t pid, uint64_t key, uint64_t val) {
    Page* pg = cache_manager_.get_page(pid);
    auto node = reinterpret_cast<BPTreeNode*>(pg->data);
    int i = node->header.num_keys - 1;
    while (i >= 0 && node->keys[i] > key) {
        node->keys[i+1] = node->keys[i];
        node->values[i+1] = node->values[i];
        i--;
    }
    node->keys[i+1] = key;
    node->values[i+1] = val;
    node->header.num_keys++;
}

void BPlusTree::split_leaf(uint32_t pid) {
    Page* pg = cache_manager_.get_page(pid);
    auto node = reinterpret_cast<BPTreeNode*>(pg->data);
    
    uint32_t new_pid = pager_.allocate_page();
    Page* new_pg = cache_manager_.get_page(new_pid);
    auto new_node = reinterpret_cast<BPTreeNode*>(new_pg->data);
    
    new_node->header.type = NodeType::LEAF;
    new_node->header.num_keys = 0;

    int mid = node->header.num_keys / 2;
    for (int i = mid; i < node->header.num_keys; i++) {
        new_node->keys[new_node->header.num_keys] = node->keys[i];
        new_node->values[new_node->header.num_keys] = node->values[i];
        new_node->header.num_keys++;
    }
    node->header.num_keys = mid;
    new_node->next_leaf = node->next_leaf;
    node->next_leaf = new_pid;

    cache_manager_.mark_dirty(pid);
    cache_manager_.mark_dirty(new_pid);
}
