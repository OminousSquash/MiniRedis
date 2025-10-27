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
#include "headers/UtilTypes.h"


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


static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
    uint8_t wbuf[k_max_msg];
    uint8_t *cur = wbuf;

    *cur++ = (uint8_t)JSON::TAG_ARR;

    uint32_t n = (uint32_t)cmd.size();
    memcpy(cur, &n, 4);
    cur += 4;

    for (const std::string &s : cmd) {
        *cur++ = (uint8_t)JSON::TAG_STR;
        uint32_t len = (uint32_t)s.size();
        memcpy(cur, &len, 4);
        cur += 4;
        memcpy(cur, s.data(), len);
        cur += len;
    }

    size_t payload_len = (size_t)(cur - wbuf);
    if (payload_len > k_max_msg) {
        msg("send_req: too big");
        return -1;
    }

    // Prepend total length (4 bytes) for TCP framing
    char final_buf[4 + k_max_msg];
    memcpy(final_buf, &payload_len, 4);
    memcpy(final_buf + 4, wbuf, payload_len);

    return write_all(fd, final_buf, 4 + payload_len);
}

static void decode_response(const uint8_t*& curr, const uint8_t* end) {
    if (curr >= end) {
        return;
    }
    JSON tag;
    memcpy(&tag, curr, 1);
    curr++;
    switch (tag) {
        case JSON::TAG_ARR: {
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

        case JSON::TAG_ERR: {
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

        case JSON::TAG_INT: {
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

        case JSON::TAG_NIL: {
            std::cout << "(nil)" << std::endl;
            break;
        }

        case JSON::TAG_STR: {
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
