#include "headers/UtilFuncs.h"

void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

void die(const char *msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

Entry* get_entry(HNode* node) {
    Entry* e = (Entry*)((char*)node - offsetof(Entry, node));
    return e;
}

Entry* get_entry_from_heap_idx(size_t* heap_idx) {
    Entry* e = (Entry*)((char*)heap_idx - offsetof(Entry, heap_idx));
    return e;
}

Conn* get_connection(Node* node) {
    Conn* connection = (Conn*) ((char*)node - offsetof(Conn, node));
    return connection;
}

uint64_t fnv_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

bool eq(HNode* left, HNode* right) {
    // check they have the same keys and hashcode
    Entry* e1 = get_entry(left);
    Entry* e2 = get_entry(right);
    return (e1->key == e2->key && left->hash_code == right->hash_code);
}

int64_t get_monotonic_msec() {
    struct timespec tv = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return uint64_t(tv.tv_sec) * 1000 + tv.tv_nsec / 1000 / 1000;
}
