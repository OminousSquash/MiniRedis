#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <assert.h>
#include "./headers/Buffer.h"

struct Conn
{
    int fd;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    Buffer read_buf;
    Buffer write_buf;
    Conn() : read_buf(), write_buf() {}
};

class Server
{
private:
    const int32_t max_read = 32 << 21;
    const int32_t max_write = 4096;
    std::vector<Conn *> conns;
    std::vector<struct pollfd> poll_args;
    int fd;

private:
    void die(const std::string &s)
    {
        std::cout << s << std::endl;
        abort();
    }

    void msg(const std::string &s)
    {
        std::cout << s << std::endl;
    }

    Conn *handle_accept()
    {
        sockaddr_in client_socket = {};
        socklen_t size = sizeof(client_socket);
        int connfd = accept(fd, (sockaddr *)&client_socket, &size);
        if (connfd < 0)
        {
            die("accept()");
            return nullptr;
        }

        fd_set_nb(connfd);

        Conn *new_conn = new Conn();
        new_conn->want_read = true;
        new_conn->fd = connfd;
        return new_conn;
    }

    void buffer_append(Buffer& buffer, const uint8_t *data, int n)
    {
        buffer.buffer_append(data, n);
    }

    void buffer_consume(Buffer& buffer, int n)
    {
        buffer.buffer_consume(n);
    }

    bool one_more_request(Conn *connection)
    {
        if (connection->read_buf.size() < 4)
        {
            return false;
        }
        int len;
        memcpy(&len, (connection->read_buf).data_begin, 4);
        if (len > max_read)
        {
            msg("message too long");
            return false;
        }
        if (4 + len > connection->read_buf.size())
        {
            return false;
        }
        uint8_t *data = (connection->read_buf).data_begin + 4;
        std::cout << "client says: " << std::string((char*)data, len) << std::endl;
        buffer_append(connection->write_buf, (uint8_t *)&len, 4);
        buffer_append(connection->write_buf, data, len);

        buffer_consume(connection->read_buf, 4 + len);
        return true;
    }

    void read_nb(Conn *connection)
    {
        uint8_t buffer[64 * 1028] = {};
        int read_bytes = read(connection->fd, buffer, sizeof(buffer));
        if (read_bytes < 0 && errno == EAGAIN)
        {
            return;
        }
        if (read_bytes < 0)
        {
            die("read()");
            return;
        }
        if (read_bytes == 0)
        {
            if (connection->read_buf.size() == 0) {
                msg("client closed");
            } else {
                msg("unexpected EOF");
            }
            connection->want_close = true;
            return;
        }
        if (read_bytes > max_read)
        {
            msg("message too long");
            return;
        }
        buffer_append(connection->read_buf, buffer, read_bytes);

        while (one_more_request(connection))
        {
        }

        if (!connection->write_buf.empty())
        {
            connection->want_read = false;
            connection->want_write = true;
            return write_nb(connection);
        }
    }

    void write_nb(Conn *connection)
    {
        assert(connection->write_buf.size() > 0);
        ssize_t bytes_written = write(connection->fd, connection->write_buf.data_begin, connection->write_buf.size());
        if (bytes_written < 0 && errno == EINTR)
        {
            return;
        }
        if (bytes_written < 0)
        {
            msg("write() error");
            connection->want_close = true;
            return;
        }
        buffer_consume(connection->write_buf, bytes_written);
        if (connection->write_buf.size() == 0)
        {
            connection->want_read = true;
            connection->want_write = false;
        }
        else
        {
            connection->want_write = true;
            connection->want_read = false;
        }
    }

    void fd_set_nb(int fd)
    {
        errno = 0;
        int flags = fcntl(fd, F_GETFL, 0);
        if (errno)
        {
            die("fcntl error");
            return;
        }

        flags |= O_NONBLOCK;

        errno = 0;
        (void)fcntl(fd, F_SETFL, flags);
        if (errno)
        {
            die("fcntl error");
        }
    }

public:
    Server()
    {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        int val = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(1234);
        server_addr.sin_addr.s_addr = htonl(0);
        int rv = bind(fd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        if (rv)
        {
            die("bind()");
        }
        fd_set_nb(fd);
        rv = listen(fd, SOMAXCONN);
        if (rv)
        {
            die("listen()");
        }
        msg("Server listening on port 1234...");
    }

    void event_loop()
    {
        while (true)
        {
            poll_args.clear();
            pollfd listening_fd = {fd, POLLIN, 0};
            poll_args.emplace_back(listening_fd);
            for (Conn *conn : conns)
            {
                if (conn == nullptr)
                {
                    continue;
                }
                pollfd conn_fd = {conn->fd, POLLERR, 0};
                if (conn->want_read)
                {
                    conn_fd.events |= POLLIN;
                }
                if (conn->want_write)
                {
                    conn_fd.events |= POLLOUT;
                }
                poll_args.push_back(conn_fd);
            }
            int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
            if (rv < 0 && errno == EINTR)
            {
                continue; // not an error
            }
            if (rv < 0)
            {
                die("poll");
            }
            // check if new connections are opened
            if (poll_args[0].revents)
            {
                Conn *new_conn = handle_accept();
                if (new_conn)
                {
                    if (new_conn->fd < conns.size()) {
                        conns[new_conn->fd] = new_conn;
                    } else {
                        conns.resize(new_conn->fd + 1);
                        conns[new_conn->fd] = new_conn;
                    }
                }
            }
            for (int i = 1; i < poll_args.size(); i++)
            {
                short ready = poll_args[i].revents;
                if (ready == 0)
                {
                    continue;
                }
                pollfd &poll_fd = poll_args[i];
                Conn *conn = conns[poll_fd.fd];
                if (ready & POLLIN)
                {
                    read_nb(conn);
                }
                if (ready & POLLOUT)
                {
                    write_nb(conn);
                }
                if (ready & POLLERR || conn->want_close)
                {
                    close(conn->fd);
                    delete conns[conn->fd];
                    conns[conn->fd] = nullptr;
                }
            }
        }
    }
};

int main()
{
    Server server;
    server.event_loop();
}
