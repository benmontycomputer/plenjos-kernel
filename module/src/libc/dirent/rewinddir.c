#include "dirent.h"

#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"

#include "errno.h"
#include "sys/syscall.h"

void rewinddir(DIR *dirp) {
    if (!dirp) {
        errno = EINVAL;
        return;
    }

    // Reset buffer
    dirp->buffer_pos = 0;
    dirp->buffer_end = 0;

    // Seek to the beginning of the directory
    ssize_t res = syscall_lseek(dirp->fd, 0, SEEK_SET);
    if (res < 0) {
        errno = -res;
        return;
    }
}