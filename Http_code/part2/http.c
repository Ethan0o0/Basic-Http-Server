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

int read_http_request(int fd, char *resource_name) {
    char buf[BUFSIZE]; //buf to read into
    int bytes_read;
    bytes_read = read(fd, buf, BUFSIZE); //reading into the buf
    if (bytes_read < 0) {
        perror("read");
        return -1;
    }
    char *ret; //getting the name found in the request
    char *space;
    ret = strchr(buf, '/'); //using strchr to get the first part and last part
    space = strchr(ret, ' ');
    int length = space - ret; //index of after name
    // printf("read_http\n");


    // memset(resource_name, '\0', sizeof(resource_name));
    strncpy(resource_name, ret, length); //copying the name to resource name
    resource_name[length] = '\0'; //used for the end of the string
    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    struct stat statbuf;
    if (stat(resource_path, &statbuf) != 0) { //using stat to check if it exists, does 404 response
        if (errno == ENOENT) {
            char buf[] = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            if (write(fd, buf, strlen(buf)) < 0)  {
                perror("write");
                return -1;
            }
        }
    }
    else {
        int file = open(resource_path, O_RDONLY); //opening
        if (file == -1) {
            perror("open");
            return -1;
        }
        int size = statbuf.st_size; //to be used for finding bytes 
        char buf[BUFSIZE]; //used for putting first part of response
        const char *ext;
        ext = strchr(resource_path, '.');
        const char *final = get_mime_type(ext); //calling this function to change to correct string
        if (final == NULL) { //error checking
            char buf[] = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            if (write(fd, buf, strlen(buf)) < 0)  {
                perror("write");
                return -1;
            }
            return 1;
        }
        sprintf(buf, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", final, size); //putting this first part initially
        if (write(fd, buf, strlen(buf)) < 0) {
            perror("write");
            return -1;
        }
        int counter = 0; //to be used in while loop condition
        int numbytes;
        int bytes;
        while(counter < size) {
            numbytes = read(file, buf, BUFSIZE); //reading into buf to finish the response
            if (numbytes < 0) {
                perror("read");
                return -1;
            }
            bytes = write(fd, buf, numbytes); //writing the buf into the file descriptor at the same time
            if (bytes < 0) {
                perror("write");
                return -1;
            }
            counter += numbytes; //adding bytes read 
        }
        if (close(file) == -1) {
            perror("close");
            return -1;
        }
    }
    return 0;
}
