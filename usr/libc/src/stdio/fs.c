#pragma once

#include "errno.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/syscall.h"
#include "sys/types.h"

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
        flags |= SYSCALL_OPEN_FLAG_READ | SYSCALL_OPEN_FLAG_WRITE;
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
        stream->flags |= __FILE_ERR;
        return 0;
    }
    if ((size_t)res < size * nmemb) {
        stream->flags |= __FILE_EOF;
    }

    return (size_t)(res / size);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    ssize_t res = syscall_write(stream->fd, ptr, size * nmemb);
    if (res < 0) {
        printf("fwrite: write error on fd %d, errno %d\n", (int)stream->fd, (int)res);
        errno = -res;
        stream->flags |= __FILE_ERR;
        return 0;
    }

    return (size_t)(res / size);
}

int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);

// TODO: do we want to buffer this or something? It seems really inefficient to do a syscall for every character.
int fgetc(FILE *stream) {
    unsigned char ch;
    ssize_t res = syscall_read(stream->fd, &ch, 1);
    if (res < 0) {
        errno = -res;
        stream->flags |= __FILE_ERR;
        return EOF;
    } else if (res == 0) {
        // EOF
        stream->flags |= __FILE_EOF;
        return EOF;
    }

    return (int)ch;
}

int fputc(int c, FILE *stream) {
    return fwrite(&c, sizeof(unsigned char), 1, stream);
}

int fputs(const char *restrict str, FILE *restrict stream) {
    return fwrite(str, sizeof(unsigned char), strlen(str), stream);
}

ssize_t getdelim(char **restrict lineptr, size_t *restrict n, int delim, FILE *restrict stream) {
    if (!lineptr || !n || !stream) {
        errno = EINVAL;
        return -1;
    }

    if (!*lineptr || *n == 0) {
        // malloc() a buffer
        *n = 128;
        *lineptr = malloc(*n);
        if (!*lineptr) {
            errno = ENOMEM;
            return -1;
        }
    }

    size_t len = 0;
    int c;
    while ((c = fgetc(stream)) != EOF) {
        if (len + 2 > *n) {
            // Not enough room for new char + terminator
            size_t cap = *n * 2;
            char *p = realloc(*lineptr, cap);
            if (!p) {
                errno = ENOMEM;
                return -1;
            }
            *lineptr = p;
            *n = cap;
        }

        (*lineptr)[len++] = (char)c;
        if (c == delim) {
            break;
        }
    }

    if (len == 0) {
        return -1;
    }

    (*lineptr)[len] = '\0';
    return (ssize_t)len;
}

ssize_t getline(char **restrict lineptr, size_t *restrict n, FILE *restrict stream) {
    return getdelim(lineptr, n, '\n', stream);
}

int ferror(FILE *stream) {
    return (stream->flags & __FILE_ERR) != 0;
}

int feof(FILE *stream) {
    return (stream->flags & __FILE_EOF) != 0;
}

void clearerr(FILE *stream) {
    stream->flags &= ~(__FILE_ERR | __FILE_EOF);
}