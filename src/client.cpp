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

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int rv = connect(fd, (const sockaddr*)&addr, sizeof(sockaddr));
    if (rv) { die("connect()"); }
    char msg[] = "hello";
    write(fd, msg, sizeof(msg));

    char read_buffer[64] = {};
    rv = read(fd, read_buffer, sizeof(read_buffer) - 1);
    if (rv < 0) {
        die("read()");
    } 
    std::cout << "server says: " << read_buffer << std::endl;
    close(fd);
}