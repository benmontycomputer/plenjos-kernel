#include "dirent.h"

#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"

#include "errno.h"
#include "sys/syscall.h"

void seekdir(DIR *dirp, long loc) {
    if (!dirp) {
        errno = EINVAL;
        return;
    }

    // Reset buffer
    dirp->buffer_pos = 0;
    dirp->buffer_end = 0;

    // Seek to the specified location
    ssize_t res = syscall_lseek(dirp->fd, loc, SEEK_SET);
    if (res < 0) {
        errno = -res;
        return;
    }
}