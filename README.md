# Multithreaded Key-Value Server

A lightweight, concurrent key-value database built from scratch in C.  
Implements low-level file persistence, interprocess communication, multithreading, and networked client access using Unix domain sockets and POSIX system calls.

---

## Features

- **Concurrent Request Handling** — spawns worker threads per connection using `pthread` for parallel client servicing.  
- **Interprocess Communication** — utilizes Unix domain sockets for local multi-client connectivity.  
- **Persistent Storage** — appends key-value pairs to a data file with atomic writes and locking (`flock`).  
- **Compaction via Signals** — triggers on `SIGUSR1` to reclaim unused disk space.  
- **Custom Commands**
  - `set <key> <value>` — insert or overwrite a key  
  - `get <key>` — retrieve a value  
  - `size` — return file size using a forked `wc` process  
  - `compact` — operates behind scenes with signaling, optimizes datastore file size on reassignment.
  - `quit` — close client connection  

---

**Core Components**
- `kvstore.c` — data operations (`set`, `get`, `size`, `compact`)
- `kvstore_server.c` — network listener, connection handler, signal setup
- `kvstore_client.c` — interactive user interface and command sender

---

## How to Run

### 1. Compile
```bash
gcc kvstore.c -c
gcc kvstore_server.c -pthread kvstore.o -o kvstore_server
gcc kvstore_client.c -o kvstore_client
```
### 2. Run the Server
```bash
Copy code
./kvstore_server
```
### 3. Run the Client
```bash
./kvstore_client
```
---

### Example Session
<img width="1688" height="745" alt="image" src="https://github.com/user-attachments/assets/49ac17b3-edb3-42b1-9721-a12d751f53f6" />

MIT License
