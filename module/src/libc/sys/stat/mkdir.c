#include "errno.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "sys/syscall.h"

int mkdir(const char *pathname, mode_t mode) {
    int res = syscall_mkdir(pathname, mode);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return 0;
}

int mkdirat(int dirfd, const char *pathname, mode_t mode) {
    if (dirfd == AT_FDCWD) {
        return mkdir(pathname, mode);
    } else {
        syscall_print("MODULE ERROR: mkdirat with dirfd other than AT_FDCWD not implemented yet.\n");
        errno = ENOSYS;
        return -1;
    }
}