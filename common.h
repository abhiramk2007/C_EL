/*
 * common.h
 *
 * Shared definitions for the LAN Messaging and File Sharing System.
 * Included by both server.c and client.c so they use the same
 * port numbers, buffer sizes, and message format prefixes.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

/* Server listens on port 8080 */
#define SERVER_PORT     8080

/* Maximum simultaneous clients the server can track */
#define MAX_CLIENTS     20

/* Buffer for reading lines and small chunks of data */
#define BUFFER_SIZE     4096

/* Maximum length of a username or filename */
#define MAX_USERNAME    50
#define MAX_FILENAME    256

/*
 * Protocol prefixes (simple text-based protocol):
 *   MSG:<username>:<message text>
 *   FILE:<username>:<filename>:<size in bytes>
 * After a FILE header line, the sender sends exactly <size> raw bytes.
 */
#define PREFIX_MSG      "MSG:"
#define PREFIX_FILE     "FILE:"
#define PREFIX_JOIN     "JOIN:"

/*
 * Client structure used by the server to track each connection.
 * socket  - TCP socket file descriptor for this client
 * username - display name chosen at connect time
 * active   - 1 if slot is in use, 0 if free
 */
typedef struct {
    int socket;
    char username[MAX_USERNAME];
    int active;
} Client;

#endif /* COMMON_H */
