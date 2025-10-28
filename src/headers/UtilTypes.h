#pragma once
#include <cstdint>
#include <vector>
#include "HashTable.h"
#include "DLL.h"
#include "Buffer.h"


enum {
    RES_OK = 0,
    RES_ERR = 1,    // error
    RES_NX = 2,     // key not found
};


enum JSON {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
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
    std::string key;
    std::string value;
};

