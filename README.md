# LAN-Based Messaging and File Sharing System using TCP Socket Programming in C

A simple academic project demonstrating **TCP sockets**, **multi-threading (pthread)**, **broadcast chat**, and **basic file transfer** on localhost.

## Project Structure

```
project/
├── common.h    # Shared constants, structures, protocol prefixes
├── server.c    # TCP server (one thread per client)
├── client.c    # TCP client (receive thread + main input loop)
├── Makefile    # Build rules
└── README.md   # This file
```

## Requirements

- GCC compiler
- POSIX environment (Linux or macOS)
- Terminal only (no GUI, no external libraries)

## Build

```bash
cd project
make
```

This produces two executables: `server` and `client`.

## Finding Your Server IP

To connect from another laptop on the same Wi-Fi/Hotspot, you need the server's local IP address.

**Windows:**
Open Command Prompt and type `ipconfig`. Look for `IPv4 Address` under your Wi-Fi or Wireless LAN adapter (e.g., `192.168.1.x`).

**macOS:**
Open Terminal and type `ifconfig en0 | grep inet` or hold Option and click the Wi-Fi icon in the menu bar.

**Linux:**
Open Terminal and type `ip a` or `hostname -I`.

## Run (Multiple Laptops)

Ensure all laptops are connected to the **same Wi-Fi network** or **Mobile Hotspot**.

### Laptop 1 (Server & Alice)

```bash
# Terminal 1 - Start Server
./server
# It will print "Listening on 0.0.0.0:8080"

# Terminal 2 - Start Client (Alice)
./client
# Enter username: Alice
# Enter Server IP: 127.0.0.1  <-- She can use loopback since it's the same machine
```

### Laptop 2 (Bob)

```bash
./client
# Enter username: Bob
# Enter Server IP: 192.168.1.15  <-- Use the IP address of Laptop 1
```

### Laptop 3 (Carol)

```bash
./client
# Enter username: Carol
# Enter Server IP: 192.168.1.15  <-- Use the IP address of Laptop 1
```

## Usage

| Input | Action |
|-------|--------|
| Any text + Enter | Send chat message to all clients |
| `/file notes.txt` | Send a file to all clients |
| `/users` | Display a list of all connected users |
| `/quit` | Disconnect and exit |

Received files are saved as `recv_<filename>` in the client's current directory.

## Example Demonstration

**Server:**

```
=== LAN Messaging Server ===
Server started
Listening on 0.0.0.0:8080
Waiting for clients...

Alice connected from 127.0.0.1
Bob connected from 192.168.1.15
```

**Alice types:** `Hello everyone`

**Bob and Carol see:**

```
[Alice] Hello everyone
```

**Bob asks for users:**

```
/users

Connected Users:
1. Alice
2. Bob
3. Carol
```

**Bob sends a file:**

```
/file sample.txt
Sending sample.txt...
File sent successfully.
```

All other clients receive:
```
Receiving sample.txt...
Saved as recv_sample.txt
```

## Protocol (Simple Text)

| Message | Format |
|---------|--------|
| Join | `JOIN:username` |
| Chat | `MSG:username:message text` |
| File header | `FILE:username:filename:size` |
| File body | Raw binary bytes (exactly `size` bytes) |

## Concepts (For Viva)

### Sockets

A **socket** is an endpoint for network communication. The server uses `socket()`, `bind()`, `listen()`, and `accept()`. The client uses `socket()` and `connect()`.

### Port

Port **8080** identifies which application on the machine receives TCP connections. `127.0.0.1` is the loopback address (same machine).

### Threads

- **Server:** One `pthread` per connected client so many clients can send data at the same time.
- **Client:** One `pthread` only for receiving, so incoming messages appear while you type.

### File Transfer

1. Client sends a header line with filename and size.
2. Client sends raw file bytes.
3. Server reads everything, then forwards header + bytes to other clients.
4. Clients save data to `recv_<filename>`.

## Clean

```bash
make clean
```

## Troubleshooting Network Problems

- **Connection Refused:** The server is not running or the IP is incorrect.
- **Connection Timeout:** The IP is wrong, or the laptops are not on the same network. Ensure they are connected to the same Wi-Fi/Mobile Hotspot.
- **Firewall Blocking:** If the server doesn't receive connections, Windows Defender Firewall or macOS Firewall might be blocking incoming connections to port 8080. Allow the port or temporarily disable the firewall for testing.

## Academic Note

This project intentionally avoids encryption, authentication, databases, and complex protocols to focus on **networking fundamentals** suitable for 2nd semester coursework.
