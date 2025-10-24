#include "headers/Buffer.h"

void Buffer::buffer_append(const uint8_t *new_data, int n) {
    if (buffer_begin == nullptr) {
        int size = std::max(n, 64);
        buffer_begin = new uint8_t[size];
        data_begin = buffer_begin;
        data_end = data_begin;
        buffer_end = buffer_begin + size;
    }
    while (data_end + n > buffer_end) {
        resize();
    }
    memcpy(data_end, new_data, n);
    data_end += n;
}

void Buffer::buffer_consume(int n) {
    data_begin += n;
    if (data_begin >= data_end) {
        data_begin = buffer_begin;
        data_end = buffer_begin;
    }
}

void Buffer::resize() {
    // double the size
    int curr_size = buffer_end - buffer_begin;
    int data_size = data_end - data_begin;
    int offset_1 = data_begin - buffer_begin;
    uint8_t* new_buffer_begin = new uint8_t[curr_size * 2];
    memcpy(new_buffer_begin,data_begin, data_size);
    delete[] buffer_begin;
    buffer_begin = new_buffer_begin;
    data_begin = buffer_begin;
    data_end = data_begin + data_size;
    buffer_end = buffer_begin + 2 * curr_size;
}

int Buffer::size() {
    return data_end-data_begin;
}

bool Buffer::empty() {
    return size() == 0;
}

void Buffer::msg(const std::string& s) {
    std::cout << s << std::endl;
}
