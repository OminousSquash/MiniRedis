#include "headers/HashTable.h"

HTable::HTable(size_t cap) {
    assert((cap & (cap - 1)) == 0);
    this->size = 0;
    this->cap = cap;
    this->mask = cap - 1;
    this->htable = new HNode*[cap]();
}

//private methods

HNode** HTable::h_lookup(HNode* target, bool (*eq)(HNode*, HNode*)) {
    int idx = target->hash_code & this->mask;
    HNode** curr = &htable[idx];
    while (*curr) {
        HNode* node = *curr;
        if (eq(node, target)) {
            return curr;
        }
        curr = &node->next;
    }
    return nullptr;
}

HNode* HTable::h_detach(HNode** from){
    HNode* delete_node = *from;
    HNode* next_node = delete_node->next;
    *from= next_node;
    size--;
    return delete_node;
}

HNode* HTable::hm_delete(HNode* target, bool (*eq)(HNode*, HNode*)) {
    HNode** from = h_lookup(target, eq);
    if (from == nullptr) {
        return nullptr;
    }
    HNode* node = h_detach(from);
    return node;
}

void HTable::h_insert(HNode* node) {
    int idx = node->hash_code & this->mask;
    node->next = htable[idx];
    htable[idx] = node;
    size++;
    if (1.0 * size / cap >= max_load_factor) {
        h_resize();
    }
}

HNode* HTable::hm_lookup(HNode* target, bool (*eq)(HNode*, HNode*)) {
    HNode** from = h_lookup(target, eq);
    if (from == nullptr) {
        return nullptr;
    }
    return *from;
}

void HTable::hm_insert(HNode* new_node) {
    h_insert(new_node);
}

void HTable::h_resize() {
    std::cout<< "RESIZE TRIGGERED" << std::endl;
    size_t old_cap = cap;
    HNode** old_table = htable;

    cap = cap << 1;
    mask = cap - 1;
    size = 0;

    htable = new HNode*[cap]();
    std::cout << "NEW TABLE ALLOCATED" << std::endl;
    for (size_t i = 0; i < old_cap; i++) {
        HNode* node = old_table[i];
        while (node != nullptr) {
            HNode* next_node = node->next;
            h_insert(node);
            node = next_node;
        }
    }
    delete [] old_table;
}
