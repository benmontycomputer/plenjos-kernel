#include "dirent.h"

#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "errno.h"
#include "sys/syscall.h"

// TODO: check scandir completely

int alphasort(const struct dirent **a, const struct dirent **b) {
    // TODO: this should be locale-aware (i.e. use strcoll instead of strcmp)
    return strcmp((*a)->d_name, (*b)->d_name);
}

int scandir(const char *dirp, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **)) {
    if (!dirp || !namelist) {
        errno = EINVAL;
        return -1;
    }

    // Open directory
    int fd = syscall_open(dirp, SYSCALL_OPEN_FLAG_DIRECTORY | SYSCALL_OPEN_FLAG_READ, 0);
    if (fd < 0) {
        errno = -fd;
        return -1;
    }

    struct dirent **entries = NULL;
    size_t entry_count = 0;
    size_t entry_capacity = 0;

    struct dirent *entry;
    while ((entry = readdir((DIR *)(uintptr_t)fd)) != NULL) {
        if (filter && !filter(entry)) {
            continue;
        }

        // Resize entries array if needed
        if (entry_count >= entry_capacity) {
            size_t new_capacity = entry_capacity == 0 ? 16 : entry_capacity * 2;
            struct dirent **new_entries = realloc(entries, new_capacity * sizeof(struct dirent *));
            if (!new_entries) {
                // Cleanup on failure
                for (size_t i = 0; i < entry_count; i++) {
                    free(entries[i]);
                }
                free(entries);
                closedir((DIR *)(uintptr_t)fd);
                errno = ENOMEM;
                return -1;
            }
            entries = new_entries;
            entry_capacity = new_capacity;
        }

        // Copy entry
        size_t name_len = strlen(entry->d_name);
        struct dirent *entry_copy = malloc(sizeof(struct dirent) + name_len + 1);
        if (!entry_copy) {
            // Cleanup on failure
            for (size_t i = 0; i < entry_count; i++) {
                free(entries[i]);
            }
            free(entries);
            closedir((DIR *)(uintptr_t)fd);
            errno = ENOMEM;
            return -1;
        }
        entry_copy->d_ino = entry->d_ino;
        strcpy(entry_copy->d_name, entry->d_name);

        entries[entry_count++] = entry_copy;
    }

    closedir((DIR *)(uintptr_t)fd);

    // Sort entries if compar function is provided
    if (compar) {
        qsort(entries, entry_count, sizeof(struct dirent *), (int (*)(const void *, const void *))compar);
    }

    *namelist = entries;
    return (int)entry_count;
}