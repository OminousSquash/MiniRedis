#pragma once
#include <cmath>
#include <vector>
#include <string>
#include "HashTable.h"
#include "DLL.h"


enum {
    RES_OK = 0,
    RES_ERR = 1,    // error
    RES_NX = 2,     // key not found
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
    std::vector<uint8_t> wb;
    std::vector<uint8_t> rb;
    uint64_t last_active_ms = 0;
    Node node;
};

struct Entry {
    HNode node;
    std::string key;
    std::string value;
};
