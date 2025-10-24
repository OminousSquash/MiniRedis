#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string>
#include <vector>
#include <map>
#include "headers/Buffer.h"
#include "headers/HashTable.h"
#include "headers/UtilTypes.h"
#include "headers/UtilFuncs.h"
#include <iostream>

class Server {
private:
    std::map<std::string, std::string> g_data;
    HTable htable;
    const size_t k_max_msg = 32 << 20;
    const size_t k_max_args = 200 * 1000;
    int fd;
private:
    void fd_set_nb(int connfd) {
        errno = 0;
        int flags = fcntl(connfd, F_GETFL, 0);
        if (errno) {
            die("fcntl error");
            return;
        }

        flags |= O_NONBLOCK;

        errno = 0;
        (void)fcntl(connfd, F_SETFL, flags);
        if (errno) {
            die("fcntl error");
        }
    }


    // append to the back
    void buf_append(std::vector<uint8_t>& buf, const uint8_t *data, size_t len) {
        buf.insert(buf.end(), data, data + len);
    }

    // remove from the front
    void buf_consume(std::vector<uint8_t>& buf, size_t len) {
        buf.erase(buf.begin(), buf.begin() + len);
    }

    // application callback when the listening socket is ready
    Conn *handle_accept() {
        // accept
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0) {
            msg_errno("accept() error");
            return NULL;
        }
        uint32_t ip = client_addr.sin_addr.s_addr;
        fprintf(stderr, "new client from %u.%u.%u.%u:%u\n",
            ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
            ntohs(client_addr.sin_port)
        );

        fd_set_nb(connfd);

        Conn *conn = new Conn();
        conn->fd = connfd;
        conn->want_read = true;
        return conn;
    }

    bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out) {
        if (cur + 4 > end) {
            return false;
        }
        memcpy(&out, cur, 4);
        cur += 4;
        return true;
    }

    bool
    read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out) {
        if (cur + n > end) {
            return false;
        }
        out.assign(cur, cur + n);
        cur += n;
        return true;
    }

    int32_t
    parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out) {
        const uint8_t *end = data + size;
        uint32_t nstr = 0;
        if (!read_u32(data, end, nstr)) {
            return -1;
        }
        if (nstr > k_max_args) {
            return -1;  // safety limit
        }

        while (out.size() < nstr) {
            uint32_t len = 0;
            if (!read_u32(data, end, len)) {
                return -1;
            }
            out.push_back(std::string());
            if (!read_str(data, end, len, out.back())) {
                return -1;
            }
        }
        if (data != end) {
            return -1;  // trailing garbage
        }
        return 0;
    }

    void make_response(const Response &resp, std::vector<uint8_t>& out) {
        uint32_t resp_len = 4 + (uint32_t)resp.data.size();
        buf_append(out, (const uint8_t *)&resp_len, 4);
        buf_append(out, (const uint8_t *)&resp.status, 4);
        buf_append(out, resp.data.data(), resp.data.size());
    }

    bool try_one_request(Conn *conn) {
        if (conn->rb.size() < 4) {
            return false;   // want read
        }
        uint32_t len = 0;
        memcpy(&len, conn->rb.data(), 4);
        if (len > k_max_msg) {
            msg("too long");
            conn->want_close = true;
            return false;   // want close
        }
        if (4 + len > conn->rb.size()) {
            return false;   // want read
        }
        const uint8_t *request = &conn->rb[4];

        std::vector<std::string> cmd;
        if (parse_req(request, len, cmd) < 0) {
            msg("bad request");
            conn->want_close = true;
            return false;   // want close
        }
        Response resp;
        do_request(cmd, resp);
        make_response(resp, conn->wb);

        buf_consume(conn->rb, 4 + len);
        return true;
    }

    void handle_write(Conn *conn) {
        assert(conn->wb.size() > 0);
        ssize_t rv = write(conn->fd, conn->wb.data(), conn->wb.size());
        if (rv < 0 && errno == EAGAIN) {
            return;
        }
        if (rv < 0) {
            msg_errno("write() error");
            conn->want_close = true;
            return;
        }

        buf_consume(conn->wb, (size_t)rv);

        if (conn->wb.size() == 0) {
            conn->want_read = true;
            conn->want_write = false;
        }
    }

    // application callback when the socket is readable
    void handle_read(Conn *conn) {
        uint8_t buf[64 * 1024];
        ssize_t rv = read(conn->fd, buf, sizeof(buf));
        if (rv < 0 && errno == EAGAIN) {
            return; // actually not ready
        }
        if (rv < 0) {
            msg_errno("read() error");
            conn->want_close = true;
            return; // want close
        }
        if (rv == 0) {
            if (conn->rb.size() == 0) {
                msg("client closed");
            } else {
                msg("unexpected EOF");
            }
            conn->want_close = true;
            return;
        }
        buf_append(conn->rb, buf, (size_t)rv);

        while (try_one_request(conn)) {}

        if (conn->wb.size() > 0) {    // has a response
            conn->want_read = false;
            conn->want_write = true;
            return handle_write(conn);
        } 
    }

    void do_get(std::string& key, Response& out) {
        msg("REACHED DO GET");
        uint64_t hash_code = fnv_hash((uint8_t*)key.data(), key.size());
        Entry e;
        e.key = key;
        e.node.hash_code = hash_code;
        HNode* result = htable.hm_lookup(&e.node, &eq);
        msg("GOT RESULT NODE");
        if (result == nullptr) {
            out.status = RES_NX;
            return;
        }
        Entry* entry = get_entry(result);
        out.status = RES_OK;
        out.data.assign(entry->value.begin(), entry->value.end());
    }

    void do_delete(std::string& key, Response& out) {
        uint64_t hash_code = fnv_hash((uint8_t*)key.data(), key.size());
        Entry e;
        e.key = key;
        e.node.hash_code = hash_code;
        HNode* result = htable.hm_delete(&e.node, &eq);
        if (result == nullptr) {
            out.status = RES_NX;
            return;
        }
        Entry* result_entry = get_entry(result);
        delete result_entry;
    }

    void do_set(std::string& key, std::string& value, Response& out) {
        uint64_t hash_code = fnv_hash((uint8_t*)key.data(), key.size());
        Entry e;
        e.key=key;
        e.node.hash_code = hash_code;
        HNode* existing_node = htable.hm_lookup(&e.node, &eq);
        if (existing_node != nullptr) {
            Entry* existing_entry = get_entry(existing_node);
            existing_entry->value = value;
            out.status = RES_OK;
        } else{
            Entry* new_entry = new Entry();
            HNode* new_node = new HNode();
            new_node->hash_code = hash_code;
            new_entry->key = key;
            new_entry->value = value;
            new_entry->node = *new_node;
            htable.hm_insert(&new_entry->node);
            out.status = RES_OK;
        }
    }

    void do_request(std::vector<std::string> &cmd, Response &out) {
        msg("REACHED DO REQ");
        if (cmd.size() == 2 && cmd[0] == "get") {
            std::string& key = cmd[1];
            do_get(key, out);
        } else if (cmd.size() == 3 && cmd[0] == "set") {
            std::string& key = cmd[1];
            std::string& value = cmd[2];
            do_set(key, value, out);
        } else if (cmd.size() == 2 && cmd[0] == "del") {
            std::string& key = cmd[1];
            do_delete(key, out);
        } else {
            out.status = RES_ERR;
        }
    }
public:
    Server() : htable(128) {}

    void run_server() {
        // the listening socket
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            die("socket()");
        }
        int val = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = ntohs(1234);
        addr.sin_addr.s_addr = ntohl(0);    // wildcard address 0.0.0.0
        int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
        if (rv) {
            die("bind()");
        }

        fd_set_nb(fd);

        rv = listen(fd, SOMAXCONN);
        if (rv) {
            die("listen()");
        }

        std::vector<Conn *> fd2conn;
        std::vector<struct pollfd> poll_args;
        while (true) {
            poll_args.clear();
            struct pollfd pfd = {fd, POLLIN, 0};
            poll_args.push_back(pfd);
            for (Conn *conn : fd2conn) {
                if (!conn) {
                    continue;
                }
                struct pollfd pfd = {conn->fd, POLLERR, 0};
                if (conn->want_read) {
                    pfd.events |= POLLIN;
                }
                if (conn->want_write) {
                    pfd.events |= POLLOUT;
                }
                poll_args.push_back(pfd);
            }

            int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
            if (rv < 0 && errno == EINTR) {
                continue;
            }
            if (rv < 0) {
                die("poll");
            }

            if (poll_args[0].revents) {
                if (Conn *conn = handle_accept()) {
                    if (fd2conn.size() <= (size_t)conn->fd) {
                        fd2conn.resize(conn->fd + 1);
                    }
                    assert(!fd2conn[conn->fd]);
                    fd2conn[conn->fd] = conn;
                }
            }

            for (size_t i = 1; i < poll_args.size(); ++i) {
                uint32_t ready = poll_args[i].revents;
                if (ready == 0) {
                    continue;
                }

                Conn *conn = fd2conn[poll_args[i].fd];
                if (ready & POLLIN) {
                    assert(conn->want_read);
                    handle_read(conn);  // application logic
                }
                if (ready & POLLOUT) {
                    assert(conn->want_write);
                    handle_write(conn); // application logic
                }

                if ((ready & POLLERR) || conn->want_close) {
                    (void)close(conn->fd);
                    fd2conn[conn->fd] = NULL;
                    delete conn;
                }
            }
        }
    }
};

int main() {
    Server s;
    s.run_server();
}
