#include "headers/UtilTypes.h"
#include <assert.h>
#include <cstdint>
#include <cstdio>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string>
#include <vector>


const size_t k_max_msg = 32<<10;

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static bool is_json(const std::string& s) {
    if (s[0] != '{') {
        return false;
    }
    int depth = 1;
    for (size_t i = 1; i < s.size(); i++) {
        if (s[i] == '{') {
            depth++;
        } else if (s[i] == '}') {
            depth--;
        }
    }
    return depth==0;
}

static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
    char k_buffer[k_max_msg] = {};
    ValueType arr_type = ValueType::ARR;
    ValueType content_type = ValueType::STR;
    size_t arr_len = cmd.size();

    // Be careful: ensure cmd has enough arguments before reading cmd[3]
    if (cmd.size() >= 3 && cmd[0] == "set" && is_json(cmd[2])) {
        content_type = ValueType::JSON;
    }

    memcpy(k_buffer, &arr_type, 1);
    memcpy(&k_buffer[1], &arr_len, 4);

    char* curr = &k_buffer[5];

    for (size_t i = 0; i < cmd.size(); i++) {
        // last element = value → may be JSON
        *curr++ = (i == cmd.size() - 1 ? content_type : ValueType::STR);

        uint32_t str_len = cmd[i].size();
        memcpy(curr, &str_len, 4);
        curr += 4;

        memcpy(curr, cmd[i].data(), str_len);
        curr += str_len;
    }

    size_t total_size = (size_t)(curr - &k_buffer[0]);

    // write the final frame: [length][payload]
    write_all(fd, (char*)&total_size, 4);
    write_all(fd, k_buffer, total_size);

    return 0;
}

static void decode_response(const uint8_t*& curr, const uint8_t* end) {
    if (curr >= end) {
        return;
    }
    ValueType tag;
    memcpy(&tag, curr, 1);
    curr++;
    switch (tag) {
        case ValueType::ARR: {
            if (curr + 4 > end) {
                msg("decode: bad str len");
                return;
            }
            uint32_t arr_len;
            memcpy(&arr_len, curr, 4);
            curr += 4;
            std::cout << "[array len=" << arr_len << "]" << std::endl;
            for (uint32_t i = 0; i < arr_len; i++) {
                decode_response(curr, end);
            }
            break;
        }

        case ValueType::ERR: {
            if (curr + 4 > end) {
                msg("decode: bad str len");
                return;
            }
            uint32_t err_msg_len;
            memcpy(&err_msg_len, curr, 4);
            curr += 4;
            if (curr + err_msg_len > end) {
                msg("decode: bad str data");
                return;
            }
            std::cout << "(str) " << std::string((const char*)curr, err_msg_len) << "\n";
            curr += err_msg_len;
            break;
        }

        case ValueType::INT: {
            if (curr + 8 > end) {
                msg("decode: bad str len");
                return;
            }
            int64_t val;
            memcpy(&val, curr, 8);
            std::cout << "(int) " << val << std::endl;
            curr += 8;
            break;
        }

        case ValueType::OK: {
            std::cout << "(ok)" << std::endl;
            break;
        }

        case ValueType::STR: {
            if (curr + 4 > end) {
                msg("decode: bad str len");
                return;
            }
            uint32_t str_len;
            memcpy(&str_len, curr, 4);
            curr += 4;
            if (curr + str_len > end) {
                msg("decode: bad str data");
                return;
            }
            std::cout << "(str) " << std::string((const char*)curr, str_len) << "\n";
            curr += str_len;
            break;
        }
        default: {
            std::cout << "unknown tag" << std::endl;
            break;
        }
    }
}

static int32_t read_res(int fd) {
    char rbuf[k_max_msg];

    // Read 4-byte length prefix (TCP framing)
    if (read_full(fd, rbuf, 4)) {
        msg("read_res: failed to read length");
        return -1;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        msg("read_res: too long");
        return -1;
    }

    // Read full payload
    if (read_full(fd, &rbuf[4], len)) {
        msg("read_res: failed to read payload");
        return -1;
    }

    const uint8_t *cur = (uint8_t*)&rbuf[4];
    const uint8_t *end = cur + len;

    std::cout << "Server response:\n";
    decode_response(cur, end);

    if (cur != end)
        msg("read_res: trailing bytes after decode");

    return 0;
}


int main(int argc, char **argv) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");
    }

    std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
    }
    int32_t err = send_req(fd, cmd);
    if (err) {
        goto L_DONE;
    }
    err = read_res(fd);
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}
