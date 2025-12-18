// POSIX
// https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/dirent.h.html

#ifndef _DIRENT_H
#define _DIRENT_H 1

#include "plenjos/dt.h" // For DT_*
#include "sys/types.h"

typedef struct {
    int fd;                     // File descriptor for the directory
    uint8_t buffer[4096];      // Buffer to hold directory entries
    size_t buffer_pos;         // Current position in the buffer
    size_t buffer_end;         // End of valid data in the buffer
} DIR;

struct dirent {
    ino_t d_ino;                 // Inode number
    char d_name[];              // Filename (null-terminated)
};

struct posix_dent {
    ino_t d_ino;                 // Inode number
    reclen_t d_reclen;           // Length of this record

    unsigned char d_type;       // Type of file
    char d_name[];              // Filename (null-terminated)
};

DIR *opendir(const char *name);
int closedir(DIR *dirp);

struct dirent *readdir(DIR *dirp);
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);

void rewinddir(DIR *dirp);
long telldir(DIR *dirp);
void seekdir(DIR *dirp, long loc);
int scandir(const char *dirp, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **));

ssize_t posix_getdents(int fd, struct posix_dent *buf, size_t nbytes, int );

DIR *fdopendir(int fd);
int dirfd(DIR *dirp);

int alphasort(const struct dirent **a, const struct dirent **b);

#endif /* _DIRENT_H */