#include <stdio.h>
#include <string.h>
#include "connection_queue.h"

int connection_queue_init(connection_queue_t *queue) {
    queue->length = 0;      //setting all to zero fist
    queue->write_idx = 0;
    queue->read_idx = 0;
    queue->shutdown = 0;
    if (pthread_mutex_init(&queue->lock, NULL) != 0) { //calling init for mutex and all conditions
        perror("mutex init");
        return -1;
    }
    if (pthread_cond_init(&queue->queue_full, NULL) != 0) {
        perror("cond init");
        return -1;
    }
    if (pthread_cond_init(&queue->queue_empty, NULL) != 0) {
        perror("cond init");
        return -1;
    }
    return 0;
}

int connection_enqueue(connection_queue_t *queue, int connection_fd) {

    if (pthread_mutex_lock(&queue->lock) != 0) { //blocking until not full
        perror("lock");
        return -1;
    }
    while (queue->length == CAPACITY && queue->shutdown != 1) { //checking if length has been reached and waiting if so
        if (pthread_cond_wait(&queue->queue_full, &queue->lock) != 0) {
            perror("cond wait");
            return -1;
        }
    }

    if (queue->shutdown == 1) { // if shutdown then we unlock and return -1
        //perror("shutdown");
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    queue->client_fds[queue->write_idx] = connection_fd; //creating a circular queue
    queue->write_idx += 1;
    queue->write_idx = queue->write_idx % CAPACITY;
    queue->length++;

    if (pthread_cond_signal(&queue->queue_empty) != 0) {//final steps of blocking
        perror("cond signal");
        return -1;
    }
    if (pthread_mutex_unlock(&queue->lock) != 0) {
        perror("unlock");
        return -1;
    }

    return 0;
}

int connection_dequeue(connection_queue_t *queue) {
    if (pthread_mutex_lock(&queue->lock) != 0) { //first lock
        perror("lock");
        return -1;
    }
    while (queue->length == 0 && queue->shutdown != 1) { //if queue is empty then block
        if (pthread_cond_wait(&queue->queue_empty, &queue->lock) != 0) {
            perror("wait");
            return -1;
        }
    }

    if (queue->shutdown == 1) { // if shutdown then we unlock and return -1
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    int temp = queue->client_fds[queue->read_idx]; //file descriptor to return
    queue->read_idx += 1;   //uses the same ciruclar queue as implemented in enqueue
    queue->read_idx = queue->read_idx % CAPACITY;
    queue->length--;

    if (pthread_cond_signal(&queue->queue_full) != 0) { //signaling here
        perror("cond signal");
        return -1;
    }
    if (pthread_mutex_unlock(&queue->lock) != 0) {//finishing the last steps by unlocking
        perror("unlock");
        return -1;
    }

    return temp;//file descriptor returned
}

int connection_queue_shutdown(connection_queue_t *queue) {
    if (pthread_mutex_lock(&queue->lock) != 0) {//first locking steps
        perror("lock");
        return -1;
    }
    queue->shutdown = 1; //setting value to 1 in the queue struct, indicating true
    if (pthread_cond_broadcast(&queue->queue_empty) != 0) { //calling broadcast on all condition queues
        perror("cond broadcast");
        return -1;
    }
    if (pthread_cond_broadcast(&queue->queue_full) != 0) {
        perror("cond broadcast");
        return -1;
    }
    if (pthread_mutex_unlock(&queue->lock) != 0) { //finishing with unlock
        perror("unlock");
        return -1;
    }
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    if (pthread_mutex_destroy(&queue->lock) != 0) { //destroying the mutex
        perror("destory mutex");
        return -1;
    }
    if (pthread_cond_destroy(&queue->queue_full) != 0) { //destorying all the conditions
        perror("destroy cond");
        return -1;
    }
    if (pthread_cond_destroy(&queue->queue_empty) != 0) {
        perror("destroy cond");
        return -1;
    }
    return 0;
}
