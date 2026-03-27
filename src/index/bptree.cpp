#include "index/bptree.h"
#include <cstring>
#include <iostream>
#include <algorithm>

BPTree::BPTree(const std::string& filename)
    : pager_(filename), cache_manager_(pager_, 256), root_pid_(0xFFFFFFFF) {
    if (pager_.get_num_pages() == 0) {
        root_pid_ = create_node(BPTNodeType::LEAF);
    } else {
        root_pid_ = 0; // Simple assumption: 0 is always root
    }
}

uint32_t BPTree::create_node(BPTNodeType type) {
    uint32_t pid = pager_.allocate_page();
    Page* p = cache_manager_.get_page(pid);
    if (!p) return 0xFFFFFFFF;
    BPTNode* node = (BPTNode*)p->data;
    memset(node, 0, 4096);
    node->header.type = type;
    node->header.num_keys = 0;
    node->header.next_leaf = 0xFFFFFFFF;
    if (type == BPTNodeType::INTERNAL) {
        for (int i = 0; i <= BPT_ORDER; i++) node->children[i] = 0xFFFFFFFF;
    }
    cache_manager_.mark_dirty(pid);
    return pid;
}

bool BPTree::search(uint64_t key, uint64_t& result) {
    uint32_t curr = root_pid_;
    while (true) {
        Page* p = cache_manager_.get_page(curr);
        if (!p) return false;
        BPTNode* node = (BPTNode*)p->data;
        if (node->header.type == BPTNodeType::LEAF) {
            for (uint32_t i = 0; i < node->header.num_keys; i++) {
                if (node->keys[i] == key) {
                    result = node->values[i];
                    return true;
                }
            }
            return false;
        }
        uint32_t i = 0;
        while (i < node->header.num_keys && key >= node->keys[i]) i++;
        curr = node->children[i];
    }
}

void BPTree::insert(uint64_t key, uint64_t row_id) {
    uint32_t leaf_pid = find_leaf(key);
    insert_into_leaf(leaf_pid, key, row_id);
}

uint32_t BPTree::find_leaf(uint64_t key) {
    uint32_t curr = root_pid_;
    while (true) {
        Page* p = cache_manager_.get_page(curr);
        if (!p) return 0xFFFFFFFF;
        BPTNode* node = (BPTNode*)p->data;
        if (node->header.type == BPTNodeType::LEAF) return curr;
        uint32_t i = 0;
        while (i < node->header.num_keys && key >= node->keys[i]) i++;
        curr = node->children[i];
    }
}

void BPTree::insert_into_leaf(uint32_t pid, uint64_t key, uint64_t val) {
    Page* p = cache_manager_.get_page(pid);
    BPTNode* node = (BPTNode*)p->data;
    
    if (node->header.num_keys < BPT_ORDER) {
        uint32_t i = node->header.num_keys;
        while (i > 0 && node->keys[i-1] > key) {
            node->keys[i] = node->keys[i-1];
            node->values[i] = node->values[i-1];
            i--;
        }
        node->keys[i] = key;
        node->values[i] = val;
        node->header.num_keys++;
        cache_manager_.mark_dirty(pid);
    } else {
        uint32_t new_pid = create_node(BPTNodeType::LEAF);
        Page* np = cache_manager_.get_page(new_pid);
        BPTNode* new_node = (BPTNode*)np->data;
        
        std::vector<std::pair<uint64_t, uint64_t>> temp;
        for (uint32_t j = 0; j < BPT_ORDER; j++) temp.push_back({node->keys[j], node->values[j]});
        temp.push_back({key, val});
        std::sort(temp.begin(), temp.end());
        
        uint32_t split_idx = (uint32_t)temp.size() / 2;
        node->header.num_keys = 0;
        for (uint32_t j = 0; j < split_idx; j++) {
            node->keys[j] = temp[j].first;
            node->values[j] = temp[j].second;
            node->header.num_keys++;
        }
        new_node->header.num_keys = 0;
        for (uint32_t j = split_idx; j < (uint32_t)temp.size(); j++) {
            new_node->keys[new_node->header.num_keys] = temp[j].first;
            new_node->values[new_node->header.num_keys] = temp[j].second;
            new_node->header.num_keys++;
        }
        new_node->header.next_leaf = node->header.next_leaf;
        node->header.next_leaf = new_pid;
        cache_manager_.mark_dirty(pid);
        cache_manager_.mark_dirty(new_pid);
        insert_into_parent(pid, new_node->keys[0], new_pid);
    }
}

void BPTree::insert_into_parent(uint32_t left_pid, uint64_t key, uint32_t right_pid) {
    if (left_pid == root_pid_) {
        uint32_t new_root_pid = create_node(BPTNodeType::INTERNAL);
        Page* rp = cache_manager_.get_page(new_root_pid);
        BPTNode* root_node = (BPTNode*)rp->data;
        root_node->keys[0] = key;
        root_node->children[0] = left_pid;
        root_node->children[1] = right_pid;
        root_node->header.num_keys = 1;
        root_pid_ = new_root_pid;
        cache_manager_.mark_dirty(new_root_pid);
        return;
    }
    
    // Find parent of left_pid
    uint32_t parent_pid = 0xFFFFFFFF;
    uint32_t curr = root_pid_;
    while (curr != 0xFFFFFFFF) {
        Page* p = cache_manager_.get_page(curr);
        BPTNode* node = (BPTNode*)p->data;
        if (node->header.type == BPTNodeType::LEAF) break;
        bool found = false;
        for (uint32_t i = 0; i <= node->header.num_keys; i++) {
            if (node->children[i] == left_pid) { parent_pid = curr; found = true; break; }
        }
        if (found) break;
        uint32_t i = 0;
        while (i < node->header.num_keys && key >= node->keys[i]) i++;
        curr = node->children[i];
    }
    
    if (parent_pid != 0xFFFFFFFF) {
        Page* pp = cache_manager_.get_page(parent_pid);
        // Added null check for parent page pp
        if (!pp) return;
        BPTNode* p_node = (BPTNode*)pp->data;
        if (p_node->header.num_keys < BPT_ORDER) {
            uint32_t i = p_node->header.num_keys;
            while (i > 0 && p_node->keys[i-1] > key) {
                p_node->keys[i] = p_node->keys[i-1];
                p_node->children[i+1] = p_node->children[i];
                i--;
            }
            p_node->keys[i] = key;
            p_node->children[i+1] = right_pid;
            p_node->header.num_keys++;
            cache_manager_.mark_dirty(parent_pid);
        }
    }
}

void BPTree::clear() {
    cache_manager_.clear();
    pager_.clear();
    root_pid_ = create_node(BPTNodeType::LEAF);
}