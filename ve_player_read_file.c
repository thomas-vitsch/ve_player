#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>

//In bytes
#define BUFFER_TYPE_LEN 4
#define BUFFER_SIZE_LEN 4

int openFile(const char *file) {
    int fd;
    
    fd = open(file, O_RDONLY);
    if (fd == -1)
        printf("Failed opening %s\n", file);
        
    return fd;
}

/* The application needs to free this buffer */
int getNextBufferFromFile(int fp, int **buf) {
    int res;

    res = read(fd, &buffer_type, 4);
    if (ret < 1)
        return ret;
        
    res = read(fd, &buffer_size, 4);
    if (ret < 1)
        return ret;
        
    if (buffer_size > 0) {
        *buf = malloc(buffer_size);
        if (buf == NULL) {
            printf("Failed allocating buffer data\n");
            return -1;
        }
        ret = read(fd, *buf, buffer_size);
}

