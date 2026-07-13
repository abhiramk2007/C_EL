

#include "common.h"

static int sockfd = -1;
static char my_username[MAX_USERNAME];
static volatile int running = 1;

/*
 * recv_all / send_all - Same idea as on server: TCP may split data.
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
 * save_received_file - Write incoming file bytes to disk.
 * Prefix filename with "recv_" to avoid overwriting local files.
 */
static void save_received_file(const char *filename, const char *data, size_t size) {
    char path[MAX_FILENAME + 16];
    snprintf(path, sizeof(path), "recv_%s", filename);

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen save file");
        return;
    }

    if (fwrite(data, 1, size, fp) != size) {
        perror("fwrite");
    }
    fclose(fp);

    printf("\nSaved as %s\n> ", path);
    fflush(stdout);
}

/*
 * receive_thread - Background thread: read messages and files from server.
 */
static void *receive_thread(void *arg) {
    (void)arg;
    char line[BUFFER_SIZE];

    while (running) {
        ssize_t n = recv(sockfd, line, sizeof(line) - 1, 0);
        if (n <= 0) {
            if (running) {
                printf("\n[Disconnected from server]\n");
            }
            running = 0;
            break;
        }

        line[n] = '\0';

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (strncmp(line, PREFIX_MSG, strlen(PREFIX_MSG)) == 0) {
            /* Display: MSG:username:text */
            char *sender = line + strlen(PREFIX_MSG);
            char *message = strchr(sender, ':');
            if (message != NULL) {
                *message = '\0';
                message++;
                if (strcmp(sender, "Server") == 0) {
                    printf("\n%s\n> ", message);
                } else {
                    printf("\n[%s] %s\n> ", sender, message);
                }
            } else {
                printf("\n%s\n> ", line);
            }
            fflush(stdout);
        }
        else if (strncmp(line, PREFIX_FILE, strlen(PREFIX_FILE)) == 0) {
            char sender[MAX_USERNAME];
            char filename[MAX_FILENAME];
            long file_size = 0;

            if (sscanf(line, "FILE:%49[^:]:%255[^:]:%ld",
                       sender, filename, &file_size) != 3) {
                printf("\n[Bad file header]\n> ");
                fflush(stdout);
                continue;
            }

            if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
                printf("\n[Invalid file size]\n> ");
                fflush(stdout);
                continue;
            }

            char *file_data = (char *)malloc((size_t)file_size);
            if (file_data == NULL) {
                perror("malloc file");
                continue;
            }

            printf("\nReceiving %s...\n> ", filename);
            fflush(stdout);

            if (recv_all(sockfd, file_data, (size_t)file_size) < 0) {
                free(file_data);
                running = 0;
                break;
            }

            save_received_file(filename, file_data, (size_t)file_size);
            free(file_data);
        }
        else {
            printf("\n%s\n> ", line);
            fflush(stdout);
        }
    }

    return NULL;
}

/*
 * send_chat_message - Wrap user text in MSG:username:... format.
 */
static void send_chat_message(const char *text) {
    char line[BUFFER_SIZE];
    snprintf(line, sizeof(line), "%s%s:%s\n", PREFIX_MSG, my_username, text);

    if (send(sockfd, line, strlen(line), 0) < 0) {
        perror("send message");
        running = 0;
    }
}

/*
 * send_file - Read a local file and send FILE header + binary data.
 */
static void send_file(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("fopen");
        return;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(fp);
        return;
    }

    long size = ftell(fp);
    if (size < 0) {
        perror("ftell");
        fclose(fp);
        return;
    }
    rewind(fp);

    if (size > 10 * 1024 * 1024) {
        printf("File too large (max 10 MB)\n");
        fclose(fp);
        return;
    }

    char *data = (char *)malloc((size_t)size);
    if (data == NULL) {
        perror("malloc");
        fclose(fp);
        return;
    }

    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        perror("fread");
        free(data);
        fclose(fp);
        return;
    }
    fclose(fp);

    /* Extract filename from path (last component after '/') */
    const char *name = strrchr(filepath, '/');
    name = (name != NULL) ? name + 1 : filepath;

    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header), "%s%s:%s:%ld\n",
             PREFIX_FILE, my_username, name, size);

    printf("Sending %s...\n", name);

    if (send(sockfd, header, strlen(header), 0) < 0 ||
        send_all(sockfd, data, (size_t)size) < 0) {
        perror("send file");
        running = 0;
    } else {
        printf("File sent successfully.\n");
    }

    free(data);
}

/*
 * main - Connect to server, start receive thread, read user input.
 */
int main(void) {
    char input[BUFFER_SIZE];

    printf("=== LAN Messaging Client ===\n");
    printf("Enter username: ");
    fflush(stdout);

    if (fgets(my_username, sizeof(my_username), stdin) == NULL) {
        return 1;
    }
    my_username[strcspn(my_username, "\n")] = '\0';

    if (strlen(my_username) == 0) {
        strcpy(my_username, "User");
    }

    char server_ip[64];
    printf("Enter Server IP: ");
    fflush(stdout);
    if (fgets(server_ip, sizeof(server_ip), stdin) == NULL) {
        return 1;
    }
    server_ip[strcspn(server_ip, "\n")] = '\0';
    if (strlen(server_ip) == 0) {
        strcpy(server_ip, "127.0.0.1");
    }

    /*
     * socket() + connect() - Client creates socket and connects to server.
     */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Connected to %s:%d as '%s'\n", server_ip, SERVER_PORT, my_username);

    /* Tell server our username */
    char join_line[BUFFER_SIZE];
    snprintf(join_line, sizeof(join_line), "%s%s\n", PREFIX_JOIN, my_username);
    if (send(sockfd, join_line, strlen(join_line), 0) < 0) {
        perror("send JOIN");
        close(sockfd);
        return 1;
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, receive_thread, NULL) != 0) {
        perror("pthread_create");
        close(sockfd);
        return 1;
    }

    printf("\nCommands:\n");
    printf("  Type a message and press Enter to chat\n");
    printf("  /file <path>   - send a file to everyone\n");
    printf("  /users         - list connected users\n");
    printf("  /quit          - exit\n\n");
    printf("> ");
    fflush(stdout);

    while (running && fgets(input, sizeof(input), stdin) != NULL) {
        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) {
            printf("> ");
            fflush(stdout);
            continue;
        }

        if (strcmp(input, "/quit") == 0) {
            running = 0;
            break;
        }

        if (strcmp(input, "/users") == 0) {
            char req[BUFFER_SIZE];
            snprintf(req, sizeof(req), "CMD:USERS\n");
            send(sockfd, req, strlen(req), 0);
            printf("> ");
            fflush(stdout);
            continue;
        }

        if (strncmp(input, "/file ", 6) == 0) {
            const char *path = input + 6;
            while (*path == ' ') {
                path++;
            }
            if (*path == '\0') {
                printf("Usage: /file <filepath>\n> ");
            } else {
                send_file(path);
                printf("> ");
            }
            fflush(stdout);
            continue;
        }

        send_chat_message(input);
        printf("> ");
        fflush(stdout);
    }

    running = 0;
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    pthread_join(tid, NULL);

    printf("Goodbye.\n");
    return 0;
}
