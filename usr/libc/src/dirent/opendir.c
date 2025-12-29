#include "dirent.h"

#include "stddef.h"
#include "stdlib.h"

#include "errno.h"
#include "sys/syscall.h"

DIR *fdopendir(int fd) {
    if (fd < 0) {
        errno = EINVAL;
        return NULL;
    }

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    if (!dir) {
        errno = ENOMEM;
        return NULL;
    }

    dir->fd = fd;
    dir->buffer_pos = 0;
    dir->buffer_end = 0;

    return dir;
}

DIR *opendir(const char *name) {
    int fd = syscall_open(name, SYSCALL_OPEN_FLAG_READ | SYSCALL_OPEN_FLAG_DIRECTORY, 0);
    if (fd < 0) {
        errno = -fd;
        return NULL;
    }

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    if (!dir) {
        syscall_close(fd);
        errno = ENOMEM;
        return NULL;
    }

    dir->fd = fd;
    dir->buffer_pos = 0;
    dir->buffer_end = 0;

    return dir;
}