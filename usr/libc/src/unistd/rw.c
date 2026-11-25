#include "errno.h"
#include "sys/syscall.h"
#include "unistd.h"

ssize_t read(int fd, void *buf, size_t count) {
    ssize_t res = syscall_read(fd, buf, count);

    if (res < 0) {
        errno = (int)-res;
        return -1;
    }

    return res;
}

ssize_t write(int fd, const void *buf, size_t count) {
    ssize_t res = syscall_write(fd, buf, count);

    if (res < 0) {
        errno = (int)-res;
        return -1;
    }

    return res;
}