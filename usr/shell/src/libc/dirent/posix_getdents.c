#include "dirent.h"

#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "errno.h"
#include "sys/syscall.h"

// TODO: check this function

ssize_t posix_getdents(int fd, struct posix_dent *buf, size_t nbytes, int ) {
    if (!buf || nbytes < sizeof(struct posix_dent)) {
        errno = EINVAL;
        return -1;
    }

    size_t total_bytes_read = 0;

    while (total_bytes_read + sizeof(struct posix_dent) <= nbytes) {
        struct dirent *entry = readdir((DIR *)(uintptr_t)fd);
        if (!entry) {
            break; // No more entries
        }

        size_t name_len = strlen(entry->d_name);
        size_t record_len = sizeof(struct posix_dent) + name_len + 1; // +1 for null terminator

        if (total_bytes_read + record_len > nbytes) {
            // Not enough space in buffer
            break;
        }

        struct posix_dent *pdent = (struct posix_dent *)((uint8_t *)buf + total_bytes_read);
        pdent->d_ino = entry->d_ino;
        pdent->d_reclen = (reclen_t)record_len;
        pdent->d_type = DT_UNKNOWN; // TODO: set correct type if possible
        memcpy(pdent->d_name, entry->d_name, name_len + 1); // Copy name with null terminator

        total_bytes_read += record_len;
    }

    return (ssize_t)total_bytes_read;
}