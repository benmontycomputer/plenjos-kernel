#include "errno.h"
#include "sys/syscall.h"
#include "unistd.h"

int chdir(const char *restrict path) {
    int res = (int)syscall_chdir(path);

    if (res < 0) {
        errno = -res;
        return -1;
    }
}

char *getcwd(char *buf, size_t size) {
    int res = (int)syscall_getcwd(buf, size);

    if (res < 0) {
        errno = -res;
        return NULL;
    }

    return buf;
}