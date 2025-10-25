#pragma once
#include <cstddef>
#include <cstdint>
#include <errno.h>
#include <time.h>
#include "UtilTypes.h"
#include "HashTable.h"

// logging / fatal
void msg(const char *msg);
void msg_errno(const char *msg);
void die(const char *msg);

// helpers to recover parent structs
Entry* get_entry(HNode* node);
Entry* get_entry_from_heap_idx(size_t* heap_idx);
Conn*  get_connection(Node* node);

// hashing / equality
uint64_t fnv_hash(const uint8_t *data, size_t len);
bool eq(HNode* left, HNode* right);

// time
int64_t get_monotonic_msec();

