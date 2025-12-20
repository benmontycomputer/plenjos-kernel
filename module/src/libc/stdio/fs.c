#pragma once

#include "sys/syscall.h"
#include "stdio.h"
#include "sys/types.h"
#include "stdlib.h"
#include "common.h"
#include "errno.h"

// TODO: implement errno
// TODO: set the stream's error indicators in addition to errno

// TODO: make this parse properly
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
    // TODO: set mode correctly instead of 0777
    int fd_res = syscall_open(filename, parse_mode(mode_str), 0777);

    if (fd_res >= 0) {
        FILE *file = (FILE *)malloc(sizeof(FILE));
        if (!file) {
            syscall_close((size_t)fd_res);
            errno = ENOMEM;
            return NULL;
        }

        file->fd = (size_t)fd_res;
        return file;
    }

    errno = -fd_res;
    return NULL;
}

int fclose(FILE *stream) {
    if (!stream) {
        errno = EINVAL;
        return EOF;
    };

    int res = syscall_close(stream->fd);
    free(stream);

    if (res < 0) {
        errno = -res;
        return EOF;
    }

    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    ssize_t res = syscall_read(stream->fd, ptr, size * nmemb);
    if (res < 0) {
        printf("fread: read error on fd %d, errno %d\n", (int)stream->fd, (int)res);
        errno = -res;
        return 0;
    }

    return (size_t)(res / size);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);

// TODO: do we want to buffer this or something? It seems really inefficient to do a syscall for every character.
int fgetc(FILE *stream) {
    unsigned char ch;
    ssize_t res = syscall_read(stream->fd, &ch, 1);
    if (res < 0) {
        printf("fgetc: read error on fd %d, errno %d\n", (int)stream->fd, (int)res);
        errno = -res;
        return EOF;
    } else if (res == 0) {
        // EOF
        return EOF;
    }

    return (int)ch;
}