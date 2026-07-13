# Makefile for LAN Messaging and File Sharing System
# Builds server and client executables using gcc

CC      = gcc
CFLAGS  = -Wall -Wextra -pthread
TARGETS = server client

.PHONY: all clean

all: $(TARGETS)

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f $(TARGETS) recv_*
