#include "headers/TTLHeap.h"
#include <utility>

void TTLHeap::update_entry_idx(HeapEntry* heap_entry, size_t new_pos) {
    *(heap_entry->heap_idx_ref) = new_pos;
}

void TTLHeap::heap_up(size_t pos) {
    while (pos > 0) {
        size_t parent_idx = get_parent_idx(pos);
        HeapEntry& curr = heap[pos];
        HeapEntry& parent = heap[parent_idx];
        if (curr.expire_time < parent.expire_time) {
            std::swap(heap[pos], heap[parent_idx]);
            update_entry_idx(&heap[pos], pos);
            update_entry_idx(&heap[parent_idx], parent_idx);
            pos = parent_idx;
        } else {
            break;
        }
    }
}

void TTLHeap::heap_down(size_t pos) {
    size_t n = heap.size();
    while (true) {
        size_t left_idx = get_left_child_idx(pos);
        size_t right_idx = get_right_child_idx(pos);
        HeapEntry& curr = heap[pos];
        size_t smallest_idx = pos;
        if (left_idx < n && heap[left_idx].expire_time < heap[smallest_idx].expire_time) {
            smallest_idx = left_idx;
        }
        if (right_idx < n && heap[right_idx].expire_time < heap[smallest_idx].expire_time) {
            smallest_idx = right_idx;
        }
        if (smallest_idx == pos) {
            break;
        }
        std::swap(curr, heap[smallest_idx]);
        update_entry_idx(&heap[pos], pos);
        update_entry_idx(&heap[smallest_idx], smallest_idx);
        pos = smallest_idx;
    }
}

size_t TTLHeap::get_parent_idx(size_t pos) {
    return ((pos - 1) / 2);
}

size_t TTLHeap::get_left_child_idx(size_t pos) {
    return (2 * (pos + 1) - 1);
}

size_t TTLHeap::get_right_child_idx(size_t pos) {
    return (2 * (pos + 1));
}

void TTLHeap::heap_update(size_t pos) {
    if (pos > 0 && heap[get_parent_idx(pos)].expire_time > heap[pos].expire_time) {
        heap_up(pos);
    } else {
        heap_down(pos);
    }
}

HeapEntry& TTLHeap::top() {
    return heap[0];
}

void TTLHeap::heap_delete() {
    if (heap.empty()) {
        return;
    }
    std::swap(heap[0], heap[heap.size() - 1]);
    update_entry_idx(&heap[0], 0);
    update_entry_idx(&heap[heap.size() - 1], heap.size() - 1);
    heap.pop_back();
    if (!heap.empty()) {
        heap_down(0);
    }
}

void TTLHeap::set_expire_time(size_t pos, uint64_t new_expire_time) {
    heap[pos].expire_time = new_expire_time;
    heap_update(pos);
}

void TTLHeap::expire_entry(size_t pos) {
    if (pos < 0 || pos >= heap.size()) {
        return;
    }
    heap[pos].expire_time = 0;
    heap_up(pos);
    heap_delete();
}

void TTLHeap::add_heap_entry(const HeapEntry& heap_entry) {
    heap.push_back(heap_entry);
    heap_up(heap.size() - 1);
}
