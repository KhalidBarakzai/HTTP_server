#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "http.h"

#define BUFSIZE 512

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    }

    return NULL;
}

int read_http_request(int fd, char *resource_name) 
{
    int sock_fd_copy = dup(fd);
    if (sock_fd_copy == -1) 
    {
        perror("dup error");
        return -1;
    }
    FILE *socket_stream = fdopen(sock_fd_copy, "r");
    if (socket_stream == NULL) 
    {
        perror("fdopen error");
        close(sock_fd_copy);
        return -1;
    }
    // Disable the usual stdio buffering
    if (setvbuf(socket_stream, NULL, _IONBF, 0) != 0) 
    {
        perror("setvbuf error");
        fclose(socket_stream);
        return -1;
    }

    // Keep consuming lines until we find an empty line
    // This marks the end of the response headers and beginning of actual content
    char bufName[BUFSIZE];
    char bufAll[BUFSIZE];
    if (fscanf(socket_stream, "GET %s HTTP/1.0\r\n", bufName) == EOF)
    {
        perror("fscanf error");
        return -1;
    }
    for(int i = 0; i < BUFSIZE; i++)
    {
        resource_name[i] = bufName[i];
    }
    while (fgets(bufAll, BUFSIZE, socket_stream) != NULL)
    {
        if (strcmp(bufAll, "\r\n") == 0) 
        {
            break;
        }
    }
    if (fclose(socket_stream) != 0) 
    {
        perror("fclose error");
        return -1;
    }
    return 0;
}


int write_http_response(int fd, const char *resource_path) 
{
    struct stat status;
    char fourohfour[BUFSIZE];
    int sock_fd_copy = fd;
    if (sock_fd_copy == -1) 
    {
        perror("dup error");
        return -1;
    }
    if ((stat(resource_path, &status)) == -1) 
    {
        if(errno == ENOENT)
        {
            if(snprintf(fourohfour, BUFSIZE, "HTTP/1.0 404 Not Found\r\nContent-Length: %d\r\n\r\n", 0) < 0)
            {
                perror("snprintf error");
                return -1;
            }
            if(write(sock_fd_copy, fourohfour, strlen(fourohfour)) == -1)
            {
                perror("write error");
                return -1;
            }
            if(close(sock_fd_copy) == -1)
            {
                perror("close error");
                return -1;
            }
            return -1;
        }
    }
    //FILE EXISTS:
    char buf[BUFSIZE];
    int bytes_read;
    char* extension = strrchr(resource_path, '.');
    if(extension == NULL)
    {
        printf("extension = NULL\r\n");
        return -1;
    }
    const char* contentType = get_mime_type(extension);
    if (contentType == NULL)
    {
        printf("contentType == NULL \r\n");
        return -1;
    }
    if(snprintf(buf, BUFSIZE, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", contentType, status.st_size) < 0)
    {
        perror("snprintf error");
        return -1;
    }
    int file = open(resource_path, O_RDONLY);
    if(file == -1)
    {
        perror("open error");
        return -1;    
    }
    if(write(sock_fd_copy, buf, strlen(buf)) == -1)
    {
        perror("write error");
        return -1;
    }
    //while it reads from file stores it into buffer: 512 chunks.
    while ((bytes_read = read(file, buf, BUFSIZE)) > 0)
    {
        if(bytes_read == -1)
        {
            perror("read error");
            return -1;
        }
         //writes buffer content into sock_fd_copy
        if (write(sock_fd_copy, buf, bytes_read) == -1)
        {
            perror("write error");
            return -1;
        }
    }
    if(close(file) == -1)
    {
        perror("close error");
        return -1;
    }
    return 0;
}
 
