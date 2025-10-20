#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>

const size_t max_read_size = 4096;

static void die(const std::string& s) {
    std::cout << s << std::endl;
    abort();
}

static void msg(const std::string& s) {
    std::cout << s << std::endl;
}

static int write_full(int fd, char* write_buffer, size_t n) {
    while (n > 0) {
        int err = write(fd, write_buffer, n);
        if (err <= 0) {
            msg("ERROR Write()");
            return -1;
        }
        n -= err;
        write_buffer += err;
    }
    return 0;
}

static int read_full(int fd, char* read_buffer, size_t n) {
    while (n > 0) {
        int err = read(fd, read_buffer, n);
        if (err <= 0) {
            msg("ERROR Read()");
            return -1;
        }
        n -= err;
        read_buffer += err;
    }
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int rv = connect(fd, (const sockaddr*)&addr, sizeof(addr));
    if (rv) { die("connect()"); }

    char message[] = "hello";
    char write_buffer[4 + strlen(message)];
    size_t len = strlen(message);
    memcpy(write_buffer, &len, 4);
    memcpy(&write_buffer[4], message, len);
    int err = write_full(fd, write_buffer, 4 + len);
    char read_buffer[4 + max_read_size] = {};
    err = read_full(fd, read_buffer, 4);
    if (err) {
        die("read()");
        return 0;
    }
    size_t message_size;
    memcpy(&message_size, read_buffer, 4);
    err = read_full(fd, &read_buffer[4], message_size);
    std::cout << "server says: " << &read_buffer[4] << std::endl;
    close(fd);
}