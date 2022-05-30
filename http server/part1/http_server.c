#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5

int keep_going = 1;

void handle_sigint(int signo) 
{
    keep_going = 0;
}

int main(int argc, char **argv) 
{

    // First command line arg is port to bind to as server
    if (argc != 3)
    {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    char *server_port = argv[2];
    char *serve_dir = argv[1];
    // Catch SIGINT so we can clean up properly
    struct sigaction sigact;
    sigact.sa_handler = handle_sigint;
    sigfillset(&sigact.sa_mask);
    sigact.sa_flags = 0; // Note the lack of SA_RESTART
    if (sigaction(SIGINT, &sigact, NULL) == -1) 
    {
        perror("sigaction");
        return 1;
    }

    // Set up hints - we'll take either IPv4 or IPv6, TCP socket type
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // We'll be acting as a server
    struct addrinfo *server;

    // Set up address info for socket() and connect()
    int ret_val = getaddrinfo(NULL, server_port, &hints, &server);
    if (ret_val != 0) 
    {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret_val));
        return 1;
    }
    // Initialize socket file descriptor
    int sock_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sock_fd == -1) 
    {
        perror("socket");
        freeaddrinfo(server);
        return 1;
    }
    // Bind socket to receive at a specific port
    if (bind(sock_fd, server->ai_addr, server->ai_addrlen) == -1) 
    {
        perror("bind");
        freeaddrinfo(server);
        close(sock_fd);
        return 1;
    }
    freeaddrinfo(server);
    // Designate socket as a server socket
    if (listen(sock_fd, LISTEN_QUEUE_LEN) == -1) 
    {
        perror("listen");
        close(sock_fd);
        return 1;
    }
    int client_fd;
    while (keep_going != 0) 
    {
        // Wait to receive a connection request from client
        // Don't both saving client address information
        printf("Waiting for client to connect\n");
        client_fd = accept(sock_fd, NULL , NULL);
        if (client_fd == -1)
        {
            if (errno != EINTR) 
            {
                perror("accept");
                close(sock_fd);
                return 1;
            }
            else 
            {
                break;
            }
        }
        printf("New client connected\n");
        char bufName[BUFSIZE];
        char bufPath[BUFSIZE];
        read_http_request(client_fd, bufName);
        strcpy(bufPath, serve_dir);
        strncat(bufPath, bufName, BUFSIZE);
        write_http_response(client_fd, bufPath);
        close(client_fd);
        printf("Client disconnected\n");
    }
    // Don't forget cleanup - reached even if we had SIGINT
    if (close(sock_fd) == -1) 
    {
        perror("close");
        return 1;
    }
    return 0;
}