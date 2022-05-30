#include <stdio.h>
#include <string.h>
#include "connection_queue.h"

int connection_queue_init(connection_queue_t *queue) 
{
    queue->length = 0;
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->shutdown = 0;
    int empty = pthread_cond_init(&queue->queue_empty, NULL);
    if(empty != 0)
    {
        fprintf(stderr, "pthread_cond_init failed: %s\n",
        strerror(empty));
        return -1;
    }
    int full = pthread_cond_init(&queue->queue_full, NULL);
    if(full != 0)
    {
        fprintf(stderr, "pthread_cond_init failed: %s\n",
        strerror(full));
        return -1;
    }
    int lock = pthread_mutex_init(&queue->lock, NULL);
    if(lock != 0)
    {
        fprintf(stderr, "pthread_mutex_init failed: %s\n",
        strerror(lock));
        return -1;
    }

    return 0; 
}

int connection_enqueue(connection_queue_t *queue, int connection_fd) 
{
    int lock = pthread_mutex_lock(&queue->lock);
    if (lock != 0)
    {
        fprintf(stderr, "pthread_mutex_lock failed: %s\n",
        strerror(lock));
        return -1;
    }
    while (queue->length == CAPACITY) 
    {
        int wait = pthread_cond_wait(&queue->queue_full, &queue->lock);
        if (wait != 0 )
        {
            fprintf(stderr, "pthread_cond_wait failed: %s\n",
            strerror(wait));
            return -1;
        }
        if(queue->shutdown == 1)
        {
            int unlock = pthread_mutex_unlock(&queue->lock);
            if(unlock != 0)
            {
                fprintf(stderr, "pthread_mutex_unlock failed: %s\n",
                strerror(unlock));
                return -1;
            }
            return -1;
        }
    }
    // Add item to queue
    queue->client_fds[queue->write_idx] = connection_fd;
    queue->write_idx =(queue->write_idx+1)%CAPACITY;
    queue->length++;
    int signal = pthread_cond_signal(&queue->queue_empty);
    if(signal != 0)
    {
        fprintf(stderr, "pthread_cond_signal failed: %s\n",
        strerror(signal));
        return -1;
    }
    int unlock = pthread_mutex_unlock(&queue->lock);
    if (unlock != 0)
    {
        fprintf(stderr, "pthread_mutex_unlock failed: %s\n",
        strerror(unlock));
        return -1;
    }
    return 0;
}

int connection_dequeue(connection_queue_t *queue) 
{
    int lock = pthread_mutex_lock(&queue->lock);
    if(lock != 0)
    {
        fprintf(stderr, "pthread_mutex_init failed: %s\n",
        strerror(lock));
        return -1;
    }
    while (queue->length == 0) 
    {
        int wait = pthread_cond_wait(&queue->queue_empty, &queue->lock);
        if (wait !=0 )
        {
            fprintf(stderr, "pthread_cond_wait failed: %s\n",
            strerror(wait));
            return -1;
        }
        if(queue->shutdown == 1)
        {
            int unlock = pthread_mutex_unlock(&queue->lock);
            if(unlock != 0)
            {
                fprintf(stderr, "pthread_mutex_unlock failed: %s\n",
                strerror(unlock));
                return -1;
            }
            return -1;
        }
    }
    // Remove item from queue
    int ret = queue->client_fds[queue->read_idx];
    queue->read_idx =(queue->read_idx+1)%CAPACITY;
    queue->length--;
    int signal = pthread_cond_signal(&queue->queue_full);
    if(signal != 0)
    {
        fprintf(stderr, "pthread_cond_signal failed: %s\n",
        strerror(signal));
        return -1;
    }
    int unlock = pthread_mutex_unlock(&queue->lock);
    if(unlock != 0)
    {
        fprintf(stderr, "pthread_mutex_unlock failed: %s\n",
        strerror(unlock));
        return -1;
    }
    return ret;
}

int connection_queue_shutdown(connection_queue_t *queue) 
{
    queue->shutdown = 1;
    int empty = pthread_cond_broadcast(&queue->queue_empty);
    if(empty != 0)
    {
        fprintf(stderr, "pthread_cond_broadcast failed: %s\n",
        strerror(empty));
        return -1;
    }
    int full = pthread_cond_broadcast(&queue->queue_full);
    if(full != 0)
    {
        fprintf(stderr, "pthread_cond_broadcast failed: %s\n",
        strerror(full));
        return -1;
    }
    return 0;
}

int connection_queue_free(connection_queue_t *queue) 
{
    int empty = pthread_cond_destroy(&queue->queue_empty);
    if(empty != 0)
    {
        fprintf(stderr, "pthread_cond_destroy failed: %s\n",
        strerror(empty));
        return -1;
    }
    int full = pthread_cond_destroy(&queue->queue_full);
    if(full != 0)
    {
        fprintf(stderr, "pthread_cond_destroy failed: %s\n",
        strerror(full));
        return -1;
    }
    int lock = pthread_mutex_destroy(&queue->lock);
    if(lock != 0)
    {
        fprintf(stderr, "pthread_cond_destroy failed: %s\n",
        strerror(lock));
        return -1;
    }
    return 0;
}
