#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;

void handle_sigint(int signo) 
{
    keep_going = 0;
}

void *thread_func(void *arg) 
{
    while(keep_going == 1)
    {
        connection_queue_t *qdpii =  (connection_queue_t*) arg;
        int client_fd = connection_dequeue(qdpii);
        if(client_fd == -1 && qdpii->shutdown == 0)
        {
            printf("error with dequeue in thread func");
        }
        if (client_fd == -1 && qdpii->shutdown == 1)
        {
            keep_going = 0;
            break;
        }
        char bufName[BUFSIZE];
        char bufPath[BUFSIZE];
        if (read_http_request(client_fd, bufName) == -1)
        {
            printf("read_http_request error");
            keep_going = 0;
            break;
        }
        strcpy(bufPath, serve_dir);
        strncat(bufPath, bufName, BUFSIZE);
        if (write_http_response(client_fd, bufPath) == -1)
        {
            printf("read_http_request error");
            keep_going = 0;
            break;
        }
        if (close(client_fd) == -1)
        {
            perror("close error");
        }
    }
    return NULL;
}

int main(int argc, char **argv) 
{
    // First command line arg is port to bind to as server
    if (argc != 3)
    {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    connection_queue_t qdpi;
    if (connection_queue_init(&qdpi) == -1)
    {
        printf("connection_queue_init error");
        return 1;
    }
    char *server_port = argv[2];
    serve_dir = argv[1];
    //signal setup to block for all worker threads and to unblock for main thread afterwards.
    sigset_t set;
    sigset_t oldset;
    if (sigfillset(&set) == -1)
    {
        perror("sigfillset error");
        return 1;
    }
    if (sigprocmask(SIG_BLOCK ,&set , &oldset) == -1)
    {
        perror("sigprocmask error");
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
        perror("socket error");
        freeaddrinfo(server);
        // {
        //     printf("freeaddrinfo error");
        // }
        return 1;
    }
    // Bind socket to receive at a specific port
    if (bind(sock_fd, server->ai_addr, server->ai_addrlen) == -1) 
    {
        perror("bind error");
        freeaddrinfo(server);
        // {
        //     printf("freeaddrinfo error");
        // }
        close(sock_fd);
        return 1;
    }
    freeaddrinfo(server);
    // Designate socket as a server socket
    if (listen(sock_fd, LISTEN_QUEUE_LEN) == -1) 
    {
        perror("listen error");
        close(sock_fd);
        return 1;
    }
    int client_fd;
    // Create multiple worker threads
    pthread_t threads[N_THREADS];
    int result;
    for (int i = 0; i < N_THREADS; i++) 
    {
        result = pthread_create(threads + i, NULL,  thread_func,  &qdpi);
        if (result != 0) 
        {
            fprintf(stderr, "pthread_create: %s\n", strerror(result));
            return 1;
        }
    }

    //UNBLOCK ALL SIGS FOR MAIN THREAD
    struct sigaction sigact;
    sigact.sa_handler = handle_sigint;
    if (sigfillset(&sigact.sa_mask) == -1)
    {
        perror("sigfillset error");
        return 1;
    }
    if (sigprocmask(SIG_SETMASK, &oldset, NULL) == -1)
    {
        perror("sigprocmask error");
        return 1;
    }
    sigact.sa_flags = 0; // Note the lack of SA_RESTART
    if (sigaction(SIGINT, &sigact, NULL) == -1) 
    {
        perror("sigaction error");
        return 1;
    }

    while (keep_going != 0) 
    {
        client_fd = accept(sock_fd, NULL , NULL);
        if (client_fd == -1)
        {
            if (errno != EINTR) 
            {
                perror("accept error");
                close(sock_fd);
                return 1;
            }
            else 
            {
                break;
            }
        }
        if (connection_enqueue(&qdpi, client_fd) == -1)
        {
            printf("connection_enqueue error");
            return 1;
        }
    }
    if (connection_queue_shutdown(&qdpi) == -1)
    {
        printf("connection_shutdown error");
        return 1;
    }
    // Join all threads
    for (int i = 0; i < N_THREADS; i++) 
    {
        if ((result = pthread_join(threads[i], NULL)) != 0) 
        {
            fprintf(stderr, "pthread_join: %s\n", strerror(result));
            return 1;
        }
    }
    if (connection_queue_free(&qdpi) == -1)
    {
        printf("connection_queue_free error");
        return 1;
    }
    if (close(sock_fd) == -1) 
    {
        perror("close error");
        return 1;
    }
    return 0;
}
