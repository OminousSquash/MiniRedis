# 🧠 MiniRedis

A lightweight, educational clone of Redis implemented in **C++**.  
MiniRedis provides a simple in-memory key-value store with **TTL (time-to-live)** support, basic persistence, and command-line client-server interaction — all from scratch.

---

## 🚀 Features

- 🗝️ **Key-Value Store** — Store and retrieve string keys and values.
- ⏱️ **TTL Support** — Keys can expire automatically after a set time.
- 🔁 **Persistence** — Convert volatile keys to persistent ones using `persist`.
- 🧩 **Custom Data Structures** — Includes a custom `HashTable`, `DLL`, and `TTLHeap`.
- 💬 **Client-Server Model** — Communicate using a simple command-based TCP protocol.

---

## 🧰 Build Instructions

Make sure you have **g++** (C++11 or later) installed.

### 1. Clone the Repository
```bash
git clone https://github.com/OminousSquash/MiniRedis.git
cd MiniRedis/src
```
### 2. Compile the Server
```bash
g++ -std=c++11 -Wall -Wextra -O2 -g Buffer.cpp HashTable.cpp server.cpp DLL.cpp TTLHeap.cpp UtilFuncs.cpp -o server
```

### 3. Compile the Client
```bash
g++ -std=c++11 -Wall -Wextra -O2 -g client.cpp -o client
```
---

## 🛠️ Usage

### 1. Start the Server
```bash
./server
```
### 2. Use the client
```bash
./client get <key1> <key2> ... <keyn> 
./client set <key> <value> <ttl>(optional)
./client del <key>
./client expire <key> <tll>
./client persist <key>
```
Example 
```
./client set foo bar 10
./client get foo
./client expire foo 20
./client persist foo
./client del foo 
```
---
## 🧠 Architecture Overview

- server.cpp — Handles incoming client connections and executes commands.

- client.cpp — CLI tool to send commands to the server.

- HashTable.cpp — Implements the core key-value store.

- TTLHeap.cpp — Manages key expiration times efficiently.

- DLL.cpp — Doubly linked list used internally for data management.

- Buffer.cpp — Handles I/O buffering.

- UtilFuncs.cpp — Helper utilities for parsing and time management.

---
## 🧩 Future Improvements

Add support for more Redis-like commands (incr, keys, flushdb, etc.)

Implement persistence to disk (RDB or AOF-style).

Add multi-threaded client handling.

