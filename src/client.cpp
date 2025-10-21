#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <string>


class Client {
private:
    const size_t max_read_size = 4096;
    const size_t max_write_size = 32 << 20;
    int fd;

private:
    void die(const std::string &s)
    {
        std::cout << s << std::endl;
        abort();
    }

    void msg(const std::string &s) { std::cout << s << std::endl; }

    int write_full(uint8_t* data, int n) {
        while (n > 0) {
            ssize_t bytes_written = write(fd,data,n);
            if (bytes_written <= 0) {
                return -1;
            }
            n -= bytes_written;
            data += bytes_written;
        }
        return 0;
    }

    int read_full(uint8_t* data, int n) {
        while (n > 0) {
            ssize_t bytes_read = read(fd, data, n);
            if (bytes_read <= 0) {
                return -1;
            }
            n -= bytes_read;
            data += bytes_read;
        }
        return 0;
    }

    int send_req(const uint8_t* s, size_t len) {
        if (len > max_write_size) {
            return -1;
        }
        std::vector<uint8_t> data;
        data.resize(4 + len);
        memcpy(&data[0], &len, 4);
        memcpy(&data[4], s, len);
        write_full((uint8_t*)data.data(), len + 4);
        return 0;
    }
    
    int read_res() {
        std::vector<uint8_t> read_buffer;
        read_buffer.resize(4);
        int err = read_full(read_buffer.data(), 4);
        if (err) {
            if (errno == 0) {
                msg("Unexpected EOF");
                return -1;
            } else {
                msg("read() error");
                return -1;
            }
        }
        int len;
        memcpy(&len, &read_buffer[0], 4);
        if (len > max_read_size) {
            msg("Exceeded max read size");
            return -1;
        }
        read_buffer.resize(4 + len);
        err = read_full(&read_buffer[4], len);
        if (err) {
            msg("read() error");
            return err;
        }
        std::cout << "len: " << len << " msg: " << std::string((char*)&read_buffer[4], len) << std::endl;
        return 0;
    }

public:
    Client() {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        int val = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(1234);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        int rv = connect(fd, (const sockaddr *)&addr, sizeof(addr));
        if (rv)
        {
            die("connect()");
        }
    }

    void send_messages() {
        std::vector<std::string> query_list = {
            "hello1",
            "hello2",
            "hello3",
            std::string(max_write_size, 'z'),
            "hello5",
        };

        for (std::string& s : query_list) {
            int err = send_req((uint8_t*)s.data(), s.size());
            if (err) {
                msg("send() error");
                close(fd);
                return;
            }
        }
        for (int i = 0; i < query_list.size(); i++) {
            int err = read_res();
            if (err) {
                msg("read() erro");
                close(fd);
                return;
            }
        }
    }
};

int main()
{
    Client client;
    client.send_messages();
}
