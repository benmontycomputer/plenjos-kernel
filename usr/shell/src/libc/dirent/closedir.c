#include "dirent.h"

#include "stdlib.h"

#include "errno.h"
#include "sys/syscall.h"

int closedir(DIR *dirp) {
    if (!dirp) {
        errno = EINVAL;
        return -1;
    }

    int res = syscall_close(dirp->fd);
    free(dirp);

    if (res < 0) {
        errno = -res;
        return -1;
    }

    return 0;
}