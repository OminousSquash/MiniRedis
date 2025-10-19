#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>

static void die(const std::string& s) {
    std::cout << s << std::endl;
    abort();
}

void do_something(int connection_fd) {
    char read_buffer[64] = {};
    ssize_t rv = read(connection_fd, read_buffer, sizeof(read_buffer) - 1);
    if (rv < 0) {
        std::cout << "read() error" << std::endl;
        return;
    }
    std::cout << "client says: " << read_buffer << std::endl;
    char write_buffer[] = "world";
    write(connection_fd, write_buffer, strlen(write_buffer));
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