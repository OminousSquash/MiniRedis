#pragma once
#include <vector>
#include "HashTable.h"
#include "DLL.h"
#include "Buffer.h"
#include <variant>

enum ValueType {
    STR = 0,
    INT = 1,
    DOUBLE = 2,
    JSON = 3,
    BIN = 4,
    ARR = 5,
    ERR = 6,
    NX = 7,
    OK = 8
};

struct Response {
    uint32_t status = 0;
    std::vector<uint8_t> data;
};

struct Conn {
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    Buffer write_buffer;
    Buffer read_buffer;
    uint64_t last_active_ms = 0;
    Node node;
};

struct HeapEntry {
    uint64_t expire_time = 0;
    size_t* heap_idx_ref = 0;
};

struct Entry {
    HNode node;
    size_t heap_idx;
    ValueType type;
    std::string value;
    std::string key;
};

