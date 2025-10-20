#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>

const int32_t max_message = 4096;

static void die(const std::string& s) {
    std::cout << s << std::endl;
    abort();
}

static void msg(const std::string& s) {
    std::cout << s << std::endl;
}

static int read_full(int connection_fd, char* read_buffer, int n) {
    while (n > 0) {
        ssize_t rv = read(connection_fd, read_buffer, n);
        if (rv <= 0)  {
            return -1;
        }
        n -= rv;
        read_buffer += rv;
    }
    return 0;
}

static int write_full(int connection_fd, char* write_buffer, int n) {
    while (n > 0) {
        ssize_t wv = write(connection_fd, write_buffer, n);
        if (wv < 0) {
            return -1;
        }
        write_buffer += wv;
        n -= wv;
    }
    return 0;
}

static int do_something(int connection_fd) {
    /*
    read from client
    check message length
    discard if length too long/error
    else read the rest of the message
    write back a message of its own
    */
    char read_buffer[4 + max_message] = {};
    int rv = read_full(connection_fd, read_buffer, 4);
    if (rv) {
        msg("READ ERROR");
        return -1;
    }
    int msg_size;
    memcpy(&msg_size, read_buffer, 4);
    if (msg_size > max_message) {
        msg("message too long");
        return -1;
    }
    
    rv = read_full(connection_fd, &read_buffer[4], msg_size);
    if (rv) {
        msg("READ ERROR");
        return -1;
    }
    std::cout << "client says: " << &read_buffer[4] << std::endl;
    char response[] = "world";
    char write_buffer[4 + strlen(response)];
    size_t response_size = strlen(response);
    memcpy(write_buffer, &response_size, 4);
    memcpy(&write_buffer[4], response, response_size);
    rv = write_full(connection_fd, write_buffer, 4 + response_size);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1234);
    server_addr.sin_addr.s_addr = htonl(0);
    int rv = bind(fd, (const struct sockaddr* )&server_addr, sizeof(server_addr));
    if (rv) { die("bind()"); }
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    msg("Server listening on port 1234...");

    while (true) {
        sockaddr_in client_addr = {};
        socklen_t addr_len = sizeof(client_addr);
        int connection_fd = accept(fd, (struct sockaddr*)&client_addr, &addr_len);
        if (connection_fd < 0) {
            continue;
        }
        do_something(connection_fd);
        close(connection_fd);
    }
}