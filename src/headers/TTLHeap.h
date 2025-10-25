#pragma once
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>
#include "UtilTypes.h"
#include "UtilFuncs.h"

class TTLHeap {
private:
    std::vector<HeapEntry> heap;

private:
    void heap_up(size_t pos);

    void heap_down(size_t pos);

    size_t get_parent_idx(size_t pos);

    size_t get_left_child_idx(size_t pos);

    size_t get_right_child_idx(size_t pos);

    void update_entry_idx(HeapEntry* heap_entry, size_t new_pos);

public:
    void heap_update(size_t pos);

    HeapEntry& top();

    size_t heap_size() {
        return heap.size();
    }

    void add_heap_entry(const HeapEntry& heap_entry);
    void heap_delete();
    void expire_entry(size_t pos);
    void set_expire_time(size_t pos, uint64_t expire_time);
    HeapEntry& operator[](size_t pos) {
        return heap[pos];
    }
};
