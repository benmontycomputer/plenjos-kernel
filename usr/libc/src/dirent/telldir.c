#include "dirent.h"

#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"

#include "errno.h"
#include "sys/syscall.h"

long telldir(DIR *dirp) {
    if (!dirp) {
        errno = EINVAL;
        return -1;
    }

    // Calculate the current position in the directory
    ssize_t res = syscall_lseek(dirp->fd, 0, SEEK_CUR);
    if (res < 0) {
        errno = -res;
        return -1;
    }

    return (long)res;
}