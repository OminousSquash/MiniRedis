#include <assert.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/_types/_u_int32_t.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string>
#include <vector>
#include "headers/Buffer.h"
#include "headers/HashTable.h"
#include "headers/UtilTypes.h"
#include "headers/UtilFuncs.h"
#include "headers/DLL.h"
#include "headers/TTLHeap.h"

static const std::string KEY_NOT_FOUND_ERROR = "key not found";
static const std::string NULL_MESSAGE = "null";
static const std::string INVALID_TTL = "ttl cannot be negative";

class Server {
private:
    HTable htable;
    DLL dll;
    TTLHeap entry_heap;
    static const size_t k_max_msg = 32 << 20;
    static const size_t k_max_args = 200 * 1000;
    static const int64_t k_tcp_idle_timeout = 5000;
    static const int64_t k_default_entry_timeout = 20000;
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


    void buf_append(Buffer& buffer, const uint8_t* data, size_t len) {
        buffer.buffer_append(data, (int) len);
    }

    void buf_consume(Buffer& buffer, size_t len) {
        buffer.buffer_consume((int) len);
    }

    void write_1b_tag(Buffer& buffer, JSON tag) {
        buffer.buffer_append((uint8_t*)&tag, 1);
    }

    void write_4b_len(Buffer& buffer, size_t size) {
        buffer.buffer_append((uint8_t*)&size, 4);
    }

    void write_int64(Buffer& buffer, int64_t val) {
        write_1b_tag(buffer, JSON::TAG_INT);
        buffer.buffer_append((uint8_t*)&val, 8);
    }

    void write_string(Buffer& buffer, const uint8_t* data, size_t len) {
        write_1b_tag(buffer, JSON::TAG_STR);
        write_4b_len(buffer, len);
        buffer.buffer_append(data, len);
    }

    void write_arr(Buffer& buffer, size_t len)  {
        write_1b_tag(buffer, JSON::TAG_ARR);
        write_4b_len(buffer, len);
    }

    void write_double(Buffer& buffer, double value) {
        write_1b_tag(buffer, JSON::TAG_DBL);
        buffer.buffer_append((uint8_t*)&value, 8);
    }

    void write_err(Buffer& buffer, const uint8_t* err_msg, size_t len) {
        write_1b_tag(buffer, JSON::TAG_ERR);
        write_4b_len(buffer, len);
        buffer.buffer_append(err_msg, len);
    }

    void write_err(Buffer& buffer) {
        write_1b_tag(buffer, JSON::TAG_ERR);
        write_4b_len(buffer, 0);
    }

    void write_success(Buffer& buffer) {
        write_1b_tag(buffer, JSON::TAG_NIL);
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
        int64_t curr_time = get_monotonic_msec();
        conn->last_active_ms = curr_time;
        dll.insert(&conn->node);
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

    bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out) {
        if (cur + n > end) {
            return false;
        }
        out.assign(cur, cur + n);
        cur += n;
        return true;
    }

    int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out) {
        const uint8_t* start = data;
        const uint8_t* end = start + size;
        if (start >= end) {
            return -1;
        }
        JSON tag;
        memcpy(&tag, data, 1);
        start++;
        if (tag != JSON::TAG_ARR) {
            msg("Expected ARR");
            return -1;
        }
        uint32_t arr_len;
        memcpy(&arr_len, start, 4);
        start += 4;
        if (arr_len > k_max_args) {
            msg("too many args");
            return -1;
        }
        for (uint32_t i = 0; i < arr_len; i++) {
            if (start >= end) {
                return -1;
            }
            memcpy(&tag, start, 1);
            start++;
            if (tag != JSON::TAG_STR) {
                msg("Expected String");
                return -1;
            }
            if (start + 4 > end) {
                msg("parse_req: unexpected end of data");
                return -1;
            }
            uint32_t str_len;
            memcpy(&str_len, start, 4);
            start += 4;
            if (start + str_len > end) {
                msg("parse_req: unexpected end of data");
                return -1;
            }
            out.emplace_back(reinterpret_cast<const char*>(start), str_len);
            start += str_len;
        }
        if (start != end) {
            msg("parse_req: trailing garbage");
            return -1;
        }
        return 0;
    }

    bool try_one_request(Conn *conn) {
        if (conn->read_buffer.size() < 4) {
            return false;   // want read
        }
        uint32_t len = 0;
        memcpy(&len, conn->read_buffer.data_begin, 4);
        if (len > k_max_msg) {
            msg("too long");
            conn->want_close = true;
            return false;   // want close
        }
        if (4 + len > (uint32_t)conn->read_buffer.size()) {
            return false;   // want read
        }
        uint8_t* data_begin = conn->read_buffer.data_begin;
        const uint8_t *request = data_begin + 4;

        std::vector<std::string> cmd;
        if (parse_req(request, len, cmd) < 0) {
            msg("bad request");
            conn->want_close = true;
            return false;   // want close
        }
        Buffer temp_buffer;
        do_request(cmd, temp_buffer);
        send_frame(temp_buffer, conn->write_buffer);
        buf_consume(conn->read_buffer, 4 + len);
        return true;
    }

    void send_frame(Buffer& temp_buffer, Buffer& write_buffer) {
        uint32_t data_len = (uint32_t)temp_buffer.size();
        write_buffer.buffer_append((uint8_t*)&data_len, 4);
        write_buffer.buffer_append(temp_buffer.data_begin, data_len);
    }

    void handle_write(Conn *conn) {
        assert(conn->write_buffer.size() > 0);
        ssize_t rv = write(conn->fd, conn->write_buffer.data_begin, conn->write_buffer.size());
        if (rv < 0 && errno == EAGAIN) {
            return;
        }
        if (rv < 0) {
            msg_errno("write() error");
            conn->want_close = true;
            return;
        }

        buf_consume(conn->write_buffer, (size_t)rv);

        if (conn->write_buffer.size() == 0) {
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
            if (conn->read_buffer.size() == 0) {
                msg("client closed");
            } else {
                msg("unexpected EOF");
            }
            conn->want_close = true;
            return;
        }
        buf_append(conn->read_buffer, buf, (size_t)rv);

        while (try_one_request(conn)) {}

        if (conn->write_buffer.size() > 0) {    // has a response
            conn->want_read = false;
            conn->want_write = true;
            return handle_write(conn);
        } 
    }

    void do_get_multi(std::vector<std::string>& keys, Buffer& write_buffer) {
        write_arr(write_buffer, keys.size());
        for (std::string& key: keys)  {
            std::cout << "GETTING KEY: " << key << std::endl;
            uint64_t hash_code = fnv_hash((uint8_t*)key.data(), key.size());
            Entry e;
            e.key = key;
            e.node.hash_code = hash_code;
            HNode* result = htable.hm_lookup(&e.node, &eq);
            if (result == nullptr) {
                write_err(write_buffer, (uint8_t*)NULL_MESSAGE.data(), NULL_MESSAGE.size());
                continue;
            }
            Entry* entry = get_entry(result);
            std::string& value = entry->value;
            write_string(write_buffer, (uint8_t*)value.data(), value.size());
        }
    }

    void do_delete(std::string& key, Buffer& buffer) {
        uint64_t hash_code = fnv_hash((uint8_t*)key.data(), key.size());
        Entry e;
        e.key = key;
        e.node.hash_code = hash_code;
        HNode* result = htable.hm_delete(&e.node, &eq);
        if (result == nullptr) {
            write_err(buffer, (uint8_t*)KEY_NOT_FOUND_ERROR.data(), KEY_NOT_FOUND_ERROR.size());
            return;
        }
        Entry* result_entry = get_entry(result);
        entry_heap.expire_entry(result_entry->heap_idx);
        delete result_entry;
        write_success(buffer);
    }

    void set_heap_entry_ttl(Entry* e, int64_t ttl) {
        int64_t expire_time = get_monotonic_msec() + ttl;
        if (ttl == (int64_t)-1) {
            entry_heap.expire_entry(e->heap_idx);
            e->heap_idx = -1;
            return;
        }
        if (e->heap_idx < entry_heap.heap_size()) {
            entry_heap.set_expire_time(e->heap_idx, expire_time);
        } else {
            HeapEntry* new_entry = new HeapEntry();
            new_entry->expire_time = expire_time;
            new_entry->heap_idx_ref = &e->heap_idx;
            entry_heap.add_heap_entry(*new_entry);
        }
    }

    void do_set(std::string& key, std::string& value, Buffer& out, int64_t ttl = k_default_entry_timeout) {
        uint64_t hash_code = fnv_hash((uint8_t*)key.data(), key.size());
        Entry e;
        e.key=key;
        e.node.hash_code = hash_code;
        HNode* existing_node = htable.hm_lookup(&e.node, &eq);
        if (existing_node != nullptr) {
            Entry* existing_entry = get_entry(existing_node);
            existing_entry->value = value;
            set_heap_entry_ttl(existing_entry, ttl);
            write_success(out);
        } else{
            Entry* new_entry = new Entry();
            HNode* new_node = new HNode();
            new_node->hash_code = hash_code;
            new_entry->key = key;
            new_entry->value = value;
            new_entry->node = *new_node;
            new_entry->heap_idx = entry_heap.heap_size();
            htable.hm_insert(&new_entry->node);
            set_heap_entry_ttl(new_entry, ttl);
            write_success(out);
        }
    }
    
    void do_persist(std::string& key, Buffer& out) {
        uint64_t hash_code = fnv_hash((uint8_t*)key.data(), key.size());
        Entry e;
        e.key = key;
        e.node.hash_code = hash_code;
        HNode* existing_node = htable.hm_lookup(&e.node, &eq);
        if (existing_node == nullptr) {
            write_err(out, (uint8_t*)KEY_NOT_FOUND_ERROR.data(), KEY_NOT_FOUND_ERROR.size());
            return;
        }
        Entry* existing_entry = get_entry(existing_node);
        entry_heap.expire_entry(existing_entry->heap_idx);
        existing_entry -> heap_idx = -1;
        write_success(out);
    }

    void do_set_expire(std::string& key, int64_t ttl, Buffer& out) {
        uint64_t hash_code = fnv_hash((uint8_t*) key.data(), key.size());
        Entry e;
        e.node.hash_code = hash_code;
        e.key = key;
        HNode* existing_node =  htable.hm_lookup(&e.node, &eq);
        if (existing_node == nullptr) {
            write_err(out, (uint8_t*)KEY_NOT_FOUND_ERROR.data(), KEY_NOT_FOUND_ERROR.size());
            return;
        }
        Entry* existing_entry = get_entry(existing_node);
        set_heap_entry_ttl(existing_entry, ttl);
        write_success(out);
    }
    
    void do_request(std::vector<std::string> &cmd, Buffer& out) {
        if (cmd.size() >= 2  && cmd[0] == "get") {
            std::vector<std::string> keys;
            for (size_t i = 1; i < cmd.size(); i++) {
                keys.push_back(cmd[i]);
            }
            do_get_multi(keys, out);
        } else if (cmd.size() == 3 && cmd[0] == "set") {
            std::string& key = cmd[1];
            std::string& value = cmd[2];
            do_set(key, value, out);
        } else if (cmd.size() == 2 && cmd[0] == "del") {
            std::string& key = cmd[1];
            do_delete(key, out);
        } else if (cmd.size() == 3 && cmd[0] == "expire") {
            std::string& key = cmd[1];
            int64_t new_ttl= std::stoll(cmd[2]);
            if (new_ttl <= 0) {
                write_err(out, (uint8_t*)INVALID_TTL.data(), INVALID_TTL.size());
                return;
            }
            do_set_expire(key, new_ttl, out);
        } else if (cmd.size() == 4 && cmd[0] == "set") {
            std::string& key = cmd[1];
            std::string& value = cmd[2];
            int64_t ttl = std::stoll(cmd[3]);
            if (ttl <= 0) {
                write_err(out, (uint8_t*)INVALID_TTL.data(), INVALID_TTL.size());
                return;
            }
            do_set(key, value, out, ttl);
        } else if (cmd.size() == 2 && cmd[0] == "persist") {
            std::string& key = cmd[1];
            do_persist(key, out);
        } else {
            write_err(out);
        }
    }

    int determine_timeout() {
        int64_t curr_time = get_monotonic_msec();
        int64_t min_expire_time = (int64_t)-1;
        if (dll.tail->prev != dll.head) {
            Node* node = dll.tail->prev;
            Conn* e = get_connection(node);
            int64_t expiry_time = e->last_active_ms + k_tcp_idle_timeout;
            min_expire_time = expiry_time; 
        }
        if (entry_heap.heap_size() > 0) {
            HeapEntry& first_entry = entry_heap.top();
            int64_t expiry_time = first_entry.expire_time;
            min_expire_time = std::min(min_expire_time, expiry_time);
        }

        if (min_expire_time == (int64_t)-1) {
            return -1;
        }
        if (min_expire_time >= curr_time) {
            return (int)(min_expire_time - curr_time);
        }
        return 0;
    }

    void handle_expired_connections(std::vector<Conn*>& fd2conn) {
        // removes old tcp connections 
        int64_t curr_time = get_monotonic_msec();
        Node* node = dll.tail->prev;
        while (node != dll.head) {
            Conn* connection = get_connection(node);
            Node* prev_node = node->prev;
            if ((int64_t)connection->last_active_ms + k_tcp_idle_timeout > curr_time) {
                return;
            }
            conn_destroy(connection, fd2conn);
            node = prev_node;
        }

        // removes old entries from entry_heap
        while (entry_heap.heap_size() > 0) {
            HeapEntry& entry = entry_heap.top();
            if (entry.expire_time > curr_time) {
                break;
            }
            Entry* e = get_entry_from_heap_idx(entry.heap_idx_ref);
            htable.hm_delete(&e->node, &eq);
            entry_heap.expire_entry(e->heap_idx);
            delete e;
        }
    }

    void conn_destroy(Conn* connection, std::vector<Conn*>& fd2conn) {
        (void)close(connection->fd);
        fd2conn[connection->fd] = NULL;
        dll.remove(&connection->node);
        delete connection;
    }

public:
    Server() : htable(4) {}

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
        addr.sin_port = htons(1234);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
            int64_t timeout = determine_timeout();
            int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), timeout);
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
                conn->last_active_ms = get_monotonic_msec();
                dll.remove(&conn->node);
                dll.insert(&conn->node);
                if (ready & POLLIN) {
                    assert(conn->want_read);
                    handle_read(conn);  // application logic
                }
                if (ready & POLLOUT) {
                    assert(conn->want_write);
                    handle_write(conn); // application logic
                }

                if ((ready & POLLERR) || conn->want_close) {
                    conn_destroy(conn, fd2conn);
                }
            }
            handle_expired_connections(fd2conn);
        }
    }
};

int main() {
    Server s;
    s.run_server();
}
