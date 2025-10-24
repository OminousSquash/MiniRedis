#include <string>
#include <iostream>
#pragma once

struct HNode{
    HNode* next = nullptr;
    uint64_t hash_code = 0;
};

class HTable {
private:
    HNode** htable;
    size_t size = 0;
    size_t mask = 0; 
    size_t cap = 0;

private:
    HNode** h_lookup(HNode* node, bool (*eq)(HNode*, HNode*));

    HNode* h_detach(HNode** from);

    void h_insert(HNode* node);
public:
    HTable(size_t size);

    HNode* hm_delete(HNode* target, bool (*eq)(HNode*, HNode*));

    HNode* hm_lookup(HNode* target, bool (*eq)(HNode*, HNode*));

    void hm_insert(HNode* new_node);
};