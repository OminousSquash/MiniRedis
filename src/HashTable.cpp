#include "headers/HashTable.h"
#include <assert.h>

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
