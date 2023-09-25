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

void handle_sigint(int signo) {
    keep_going = 0;
}

void *thread_pool_func(void *arg) { //my void function that I will pass in into the thread create
        connection_queue_t *temp = (connection_queue_t *) arg; //queue
        char temp1[BUFSIZE]; //buf to read into
        while (1) {
            int fd = connection_dequeue(temp); //calling dequeue to get the file descriptor
            if (fd == -1) {
                // perror("dequeue");
                return NULL;
            }
            // printf("accept passes\n");
            if (read_http_request(fd, temp1) == -1) { //reading the request into the file descriptor 
                perror("read");
                close(fd);
                return NULL;
            }
            // printf("read passes\n");
            char new[BUFSIZE]; //another buf to put the entire string we want 
            if (snprintf(new, BUFSIZE, "%s%s", serve_dir, temp1) < 0) { //assembling the name with the directory name into new
                perror("snprintf");
                return NULL;
            }  
            if (write_http_response(fd, new) == -1) { //writing to the file descriptor
                perror("write");
                close(fd);
                return NULL;
            }
            if (close(fd) == -1) { //closing it off
                perror("close");
                return NULL;
            }
            // printf("void func passes\n");
        }
    //     if (connection_queue_shutdown(temp) == -1) {
    //         perror("shutdown\n");
    //         return NULL;
    // }
        return NULL;
    }

int main(int argc, char **argv) {
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return -1;
    }

    connection_queue_t queue; //initializing a connection queue
    if (connection_queue_init(&queue) == -1) {
        perror("init");
        return -1;
    }


    serve_dir = argv[1];    //the commands that will be passed in
    const char *port = argv[2];

    // Catch SIGINT so we can clean up properly
    struct sigaction sigact;
    sigact.sa_handler = handle_sigint;
    sigfillset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    // Set up hints - we'll take either IPv4 or IPv6, TCP socket type
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // We'll be acting as a server
    struct addrinfo *server;

    // Set up address info for socket() and connect()
    int ret_val = getaddrinfo(NULL, port, &hints, &server);
    if (ret_val != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret_val));
        return -1;
    }
    // Initialize socket file descriptor
    int sock_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sock_fd == -1) {
        perror("socket");
        freeaddrinfo(server);
        return -1;
    }
    // Bind socket to receive at a specific port
    if (bind(sock_fd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(server);
        close(sock_fd);
        return -1;
    }
    freeaddrinfo(server);
    // Designate socket as a server socket
    if (listen(sock_fd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        close(sock_fd);
        return -1;
    }

    //making threads
    pthread_t threads[N_THREADS]; //threads array
    sigset_t curr; //creating new sigsets so that we can block and save them
    sigset_t old;
    sigfillset(&curr); //filling the current set 
    if (sigprocmask(SIG_BLOCK, &curr, &old) == -1) { //blocking all signals
        perror("mask1");
        return -1;
    }
     for (int i = 0; i < N_THREADS; i++) { //creating thread pool
        int result = pthread_create(threads + i, NULL, thread_pool_func, &queue); //passing void func from earlier
        if (result != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(result));
            connection_queue_shutdown(&queue);
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            
            connection_queue_free(&queue);
            return 1;
        }
    }
    if (sigprocmask(SIG_SETMASK, &old, NULL) == -1) { //putting back the old mask saved 
        perror("mask");
        return -1;
    }

    while(keep_going != 0) {
        int client_fd = accept(sock_fd, NULL, NULL); //calling accept
        if (client_fd == -1) {
            if (errno != EINTR) { //error checking
                perror("accept");
                close(sock_fd);
                return 1;
            } else {
                break;
            }
        }
        // if (queue.shutdown == 1) {
        //     break;
        // }
        int ret = connection_enqueue(&queue, client_fd);//adding to connection queue
        if (ret == -1) {
            perror("enqueue");
            return 1;
        }
    }
    //while loop finished
    if (connection_queue_shutdown(&queue) == -1) { //shutting down the connection queue
        perror("shutdown\n");
        return -1;
    }
    
    int result =  0;
    for (int i = 0; i < N_THREADS; i++) { //joining all the threads at the end
        if ((result = pthread_join(threads[i], NULL)) != 0) {
            // printf("threads\n");
            perror("join");
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            return 1;
        }
    }
    if (connection_queue_free(&queue) != 0) { //freeing the connection queue
        perror("free");
        return -1;
    }


    return 0;
}
