#include <string>
#include <iostream>

#ifndef BUFFER_H
#define BUFFER_H

class Buffer {
public:
    uint8_t* buffer_begin;
    uint8_t* data_begin;
    uint8_t* data_end;
    uint8_t* buffer_end;

private:
    void resize();

    void msg(const std::string& s);

public:
    Buffer() : buffer_begin(nullptr), data_begin(nullptr), data_end(nullptr), buffer_end(nullptr) 
    {
    }

    ~Buffer() {
        delete[] buffer_begin;
    }

    void buffer_consume(int n);

    void buffer_append(const uint8_t *new_data, int n);

    int size();

    bool empty();
};

#endif