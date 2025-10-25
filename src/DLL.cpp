#include "headers/DLL.h"

void DLL::insert(Node* node) {
    Node* next_node = head->next;
    head->next = node;
    node->next = next_node;
    node->prev = head;
    next_node->prev = node;
}

void DLL::remove(Node* node) {
    Node* prev_node = node->prev;
    Node* next_node = node->next;
    prev_node -> next = next_node;
    next_node -> prev = prev_node;
}