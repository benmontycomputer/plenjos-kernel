#pragma once

#include "sys/syscall.h"
#include "stdio.h"
#include "types.h"
#include "stdlib.h"
#include "common.h"

FILE *fopen(const char *filename, const char *mode) {
    ssize_t fd_res = syscall_open(filename, mode);

    if (fd_res >= 0) {
        FILE *file = (FILE *)malloc(sizeof(FILE));
        if (!file) {
            syscall_close((size_t)fd_res);
            return NULL;
        }

        file->fd = (size_t)fd_res;
        return file;
    }

    return NULL;
}

int fclose(FILE *stream) {
    if (!stream) return -1;

    syscall_close(stream->fd);
    free(stream);

    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    ssize_t res = syscall_read(stream->fd, ptr, size * nmemb);
    if (res < 0) return 0;

    return (size_t)(res / size);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);