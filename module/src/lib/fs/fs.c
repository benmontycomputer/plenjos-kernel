#pragma once

#include "plenjos/syscall.h"
#include "lib/stdio.h"
#include "lib/types.h"
#include "lib/stdlib.h"
#include "lib/common.h"

FILE *fopen(const char *filename, const char *mode) {
    ssize_t fd_res = (ssize_t)syscall(SYSCALL_OPEN, (uint64_t)filename, (uint64_t)mode, 0, 0, 0);

    if (fd_res >= 0) {
        FILE *file = (FILE *)malloc(sizeof(FILE));
        if (!file) {
            syscall(SYSCALL_CLOSE, (uint64_t)fd_res, 0, 0, 0, 0);
            return NULL;
        }

        file->fd = (size_t)fd_res;
        return file;
    }

    return NULL;
}

int fclose(FILE *stream) {
    if (!stream) return -1;

    syscall(SYSCALL_CLOSE, stream->fd, 0, 0, 0, 0);
    free(stream);

    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    ssize_t res = (ssize_t)syscall(SYSCALL_READ, stream->fd, (uint64_t)ptr, size * nmemb, 0, 0);
    if (res < 0) return 0;

    return (size_t)(res / size);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);