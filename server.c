/*
 * server.c
 *
 * TCP chat and file-sharing server.
 * - Binds to 127.0.0.1:8080
 * - Accepts multiple clients
 * - Creates one pthread per client
 * - Broadcasts text messages and files to all connected clients
 */

#include "common.h"

/* Global list of connected clients (protected by clients_mutex) */
static Client clients[MAX_CLIENTS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * recv_all - Read exactly 'count' bytes from socket into buffer.
 * TCP is a stream: one recv() may return less than requested.
 * Returns 0 on success, -1 on error or disconnect.
 */
static int recv_all(int sock, void *buffer, size_t count) {
    size_t total = 0;
    char *buf = (char *)buffer;

    while (total < count) {
        ssize_t n = recv(sock, buf + total, count - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

/*
 * send_all - Send exactly 'count' bytes from buffer to socket.
 * Returns 0 on success, -1 on error.
 */
static int send_all(int sock, const void *buffer, size_t count) {
    size_t total = 0;
    const char *buf = (const char *)buffer;

    while (total < count) {
        ssize_t n = send(sock, buf + total, count - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

/*
 * add_client - Register a new client in the global array.
 * Returns client index, or -1 if server is full.
 */
static int add_client(int sock, const char *username) {
    int index = -1;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].socket = sock;
            strncpy(clients[i].username, username, MAX_USERNAME - 1);
            clients[i].username[MAX_USERNAME - 1] = '\0';
            clients[i].active = 1;
            index = i;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    return index;
}

/*
 * remove_client - Mark a client slot as free when they disconnect.
 */
static void remove_client(int index) {
    pthread_mutex_lock(&clients_mutex);

    if (index >= 0 && index < MAX_CLIENTS) {
        clients[index].active = 0;
        clients[index].socket = -1;
        clients[index].username[0] = '\0';
    }

    pthread_mutex_unlock(&clients_mutex);
}

/*
 * broadcast_line - Send one text line (with newline) to every active client
 * except optionally the sender (exclude_index = -1 means send to all).
 */
static void broadcast_line(const char *line, int exclude_index) {
    char buffer[BUFFER_SIZE];
    size_t len = strlen(line);

    if (len + 2 >= sizeof(buffer)) {
        return;
    }

    snprintf(buffer, sizeof(buffer), "%s\n", line);

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            continue;
        }
        if (i == exclude_index) {
            continue;
        }
        if (send(clients[i].socket, buffer, strlen(buffer), 0) < 0) {
            perror("broadcast_line send");
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/*
 * handle_file - Receive file data from one client and broadcast to others.
 * Header format: FILE:username:filename:size
 */
static void handle_file(int sender_sock, int sender_index,
                        const char *username, const char *filename,
                        long file_size) {
    char *file_buffer;
    char header[BUFFER_SIZE];

    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
        printf("[Server] Rejected file (invalid size: %ld)\n", file_size);
        return;
    }

    file_buffer = (char *)malloc((size_t)file_size);
    if (file_buffer == NULL) {
        perror("[Server] malloc for file");
        return;
    }

    printf("[Server] Receiving file '%s' (%ld bytes) from %s\n",
           filename, file_size, username);

    if (recv_all(sender_sock, file_buffer, (size_t)file_size) < 0) {
        printf("[Server] Failed to receive file from %s\n", username);
        free(file_buffer);
        return;
    }

    snprintf(header, sizeof(header), "%s%s:%s:%ld",
             PREFIX_FILE, username, filename, file_size);

    /* Send header line then file bytes to every other client */
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active || i == sender_index) {
            continue;
        }

        char line[BUFFER_SIZE];
        snprintf(line, sizeof(line), "%s\n", header);
        send(clients[i].socket, line, strlen(line), 0);
        send_all(clients[i].socket, file_buffer, (size_t)file_size);
    }

    pthread_mutex_unlock(&clients_mutex);

    pthread_mutex_unlock(&clients_mutex);

    free(file_buffer);
}

/*
 * handle_client - Thread function: read commands from one client until disconnect.
 * arg is a heap-allocated int* with the client index.
 */
static void *handle_client(void *arg) {
    int client_index = *(int *)arg;
    free(arg);

    int sock;
    char line[BUFFER_SIZE];
    char username[MAX_USERNAME];

    pthread_mutex_lock(&clients_mutex);
    sock = clients[client_index].socket;
    strncpy(username, clients[client_index].username, MAX_USERNAME - 1);
    username[MAX_USERNAME - 1] = '\0';
    pthread_mutex_unlock(&clients_mutex);



    while (1) {
        ssize_t n = recv(sock, line, sizeof(line) - 1, 0);
        if (n <= 0) {
            break;
        }

        line[n] = '\0';

        /* Remove trailing newline if present */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (strncmp(line, PREFIX_MSG, strlen(PREFIX_MSG)) == 0) {
            /* Chat message: broadcast to everyone (including sender for echo) */
            broadcast_line(line, -1);
        }
        else if (strncmp(line, PREFIX_FILE, strlen(PREFIX_FILE)) == 0) {
            /*
             * File header: FILE:username:filename:size
             * sscanf reads the fields; binary data follows on the socket.
             */
            char file_user[MAX_USERNAME];
            char filename[MAX_FILENAME];
            long file_size = 0;

            if (sscanf(line, "FILE:%49[^:]:%255[^:]:%ld",
                       file_user, filename, &file_size) == 3) {
                handle_file(sock, client_index, file_user, filename, file_size);
            } else {
                printf("[Server] Bad FILE header: %s\n", line);
            }
        }
        else if (strcmp(line, "CMD:USERS") == 0) {
            char resp[BUFFER_SIZE * 4] = "MSG:Server:Connected Users:\n";
            int count = 1;
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].active) {
                    char temp[MAX_USERNAME + 10];
                    snprintf(temp, sizeof(temp), "%d. %s\n", count++, clients[i].username);
                    strcat(resp, temp);
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            strcat(resp, "\n");
            send(sock, resp, strlen(resp), 0);
        }
        else {
            // Unknown command
        }
    }

    printf("%s disconnected.\n", username);

    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "MSG:Server:*** %s left the chat ***", username);
    broadcast_line(leave_msg, client_index);

    close(sock);
    remove_client(client_index);
    return NULL;
}

/*
 * main - Create listening socket, accept clients, spawn threads.
 */
int main(void) {
    int server_fd;
    int client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    /* Initialize client table */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].active = 0;
        clients[i].socket = -1;
        clients[i].username[0] = '\0';
    }

    /*
     * socket() - Create a TCP (SOCK_STREAM) IPv4 socket.
     * AF_INET = IPv4, SOCK_STREAM = reliable TCP connection.
     */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    /* Allow quick restart after server stops (reuse port immediately) */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    /*
     * bind() - Attach socket to IP address and port number.
     * listen() - Mark socket as passive (waiting for connections).
     */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("=== LAN Messaging Server ===\n");
    printf("Server started\n");
    printf("Listening on 0.0.0.0:%d\n", SERVER_PORT);
    printf("Waiting for clients...\n\n");

    while (1) {
        /*
         * accept() - Block until a client connects.
         * Returns a new socket dedicated to that client.
         */
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        /* First message must be JOIN:username */
        char join_line[BUFFER_SIZE];
        ssize_t n = recv(client_fd, join_line, sizeof(join_line) - 1, 0);
        if (n <= 0) {
            close(client_fd);
            continue;
        }
        join_line[n] = '\0';

        char username[MAX_USERNAME] = "Anonymous";
        if (sscanf(join_line, "JOIN:%49[^\n\r]", username) != 1) {
            printf("[Server] Invalid JOIN, closing connection\n");
            close(client_fd);
            continue;
        }

        int index = add_client(client_fd, username);
        if (index < 0) {
            printf("Server full, rejecting %s\n", username);
            close(client_fd);
            continue;
        }

        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, sizeof(client_ip_str));
        printf("%s connected from %s\n", username, client_ip_str);

        char join_msg[BUFFER_SIZE];
        snprintf(join_msg, sizeof(join_msg), "MSG:Server:*** %s joined the chat ***", username);
        broadcast_line(join_msg, -1);

        /* Spawn one thread per client for concurrent handling */
        int *index_ptr = malloc(sizeof(int));
        if (index_ptr == NULL) {
            perror("malloc");
            remove_client(index);
            close(client_fd);
            continue;
        }
        *index_ptr = index;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, index_ptr) != 0) {
            perror("pthread_create");
            free(index_ptr);
            remove_client(index);
            close(client_fd);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
