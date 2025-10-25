#pragma once

struct Node {
    Node* next = nullptr;
    Node* prev = nullptr;
};

class DLL {
public:
    Node* head;
    Node* tail;
public:
    DLL() {
        head = new Node();
        tail = new Node();
        head->next = tail;
        tail->prev = head;
    }

    void remove(Node* node);

    void insert(Node* node);
};
