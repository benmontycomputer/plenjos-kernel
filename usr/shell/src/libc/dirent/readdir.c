#include "dirent.h"

#include "stddef.h"
#include "stdlib.h"
#include "string.h"

#include "errno.h"
#include "sys/syscall.h"

struct dirent *readdir(DIR *dirp) {
    if (!dirp) {
        errno = EINVAL;
        return NULL;
    }

    // If buffer is empty, fill it
    if (dirp->buffer_pos >= dirp->buffer_end) {
        ssize_t res = syscall_getdents(dirp->fd, (struct plenjos_dirent *)dirp->buffer, sizeof(dirp->buffer));
        if (res < 0) {
            errno = -res;
            return NULL;
        }

        if (res == 0) {
            // End of directory
            return NULL;
        }

        dirp->buffer_pos = 0;
        dirp->buffer_end = (size_t)res;
    }

    // Read the next entry from the buffer
    struct plenjos_dirent *entry_plenjos = (struct plenjos_dirent *)(dirp->buffer + dirp->buffer_pos);
    size_t entry_size = sizeof(struct plenjos_dirent);

    dirp->buffer_pos += entry_size;

    struct dirent *entry = (struct dirent *)malloc(sizeof(struct dirent) + strlen(entry_plenjos->d_name) + 1);
    if (!entry) {
        errno = ENOMEM;
        return NULL;
    }

    entry->d_ino = 0; // TODO: set inode number if available
    strcpy(entry->d_name, entry_plenjos->d_name);

    return entry;
}

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result) {
    if (!dirp || !entry || !result) {
        return EINVAL;
    }

    struct dirent *next_entry = readdir(dirp);
    if (!next_entry) {
        *result = NULL;
        return 0; // End of directory or error
    }

    // Copy the entry data to the provided entry structure
    size_t name_len = strlen(next_entry->d_name) + 1; // +1 for null terminator
    entry->d_ino = next_entry->d_ino;
    memcpy(entry->d_name, next_entry->d_name, name_len);

    *result = entry;
    return 0;
}