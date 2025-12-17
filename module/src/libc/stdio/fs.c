#pragma once

#include "sys/syscall.h"
#include "stdio.h"
#include "types.h"
#include "stdlib.h"
#include "common.h"

// TODO: implement errno

syscall_open_flags_t parse_mode(const char *mode_str) {
    syscall_open_flags_t flags = 0;

    if (mode_str[0] == 'r') {
        flags |= SYSCALL_OPEN_FLAG_READ;
    }
    if (mode_str[0] == 'w') {
        flags |= SYSCALL_OPEN_FLAG_WRITE | SYSCALL_OPEN_FLAG_CREATE;
    }
    // Append not yet supported
    // TODO: implement append
    /* if (mode_str[0] == 'a') {
        flags |= SYSCALL_OPEN_FLAG_APPEND | SYSCALL_OPEN_FLAG_CREATE;
    } */

    if (mode_str[0] != '\0' && mode_str[1] == '+') {
        flags |=  SYSCALL_OPEN_FLAG_READ | SYSCALL_OPEN_FLAG_WRITE;
    }

    return flags;
}

FILE *fopen(const char *filename, const char *mode_str) {
    ssize_t fd_res = syscall_open(filename, parse_mode(mode_str));

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
    if (res < 0) {
        printf("fread: read error on fd %d, errno %d\n", (int)stream->fd, (int)res);
        return 0;
    }

    return (size_t)(res / size);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);