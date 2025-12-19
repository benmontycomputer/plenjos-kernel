// POSIX
// https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/sys_stat.h.html

#ifndef _SYS_STAT_H
#define _SYS_STAT_H 1

#include "plenjos/posix_stat.h"

#include "sys/types.h"
// TODO: Include this once implemented
// #include "time.h"

// TODO: remove this once time.h is implemented
struct timespec {
    time_t tv_sec;        /* seconds */
    long   tv_nsec;       /* nanoseconds */
};

struct stat {
    dev_t st_dev;     /* Device ID of device containing file */
    ino_t st_ino;     /* File serial number */
    mode_t st_mode;   /* File type and mode */
    nlink_t st_nlink; /* Number of hard links */
    uid_t st_uid;     /* User ID of owner */
    gid_t st_gid;     /* Group ID of owner */
    dev_t st_rdev;    /* Device ID (if special file) */

    off_t st_size; /* Total size, in bytes */

    blksize_t st_blksize; /* Preferred I/O block size */
    blkcnt_t st_blocks;   /* Number of 512B blocks allocated */

    struct timespec st_atim; /* Last access */
    struct timespec st_mtim; /* Last modification */
    struct timespec st_ctim; /* Last status change */
};

/* POSIX compatibility macros */
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

/* POSIX test macros */
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)  /* Block device */
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)  /* Character device */
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)  /* Directory */
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO) /* FIFO or pipe */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)  /* Regular file */
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)  /* Symbolic link */
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK) /* Socket */

// TODO: do we want to implement these?
/* More POSIX test macros */
#define S_TYPEISMQ(buf)  (0) /* Message queue */
#define S_TYPEISSEM(buf) (0) /* Semaphore */
#define S_TYPEISSHM(buf) (0) /* Shared memory object */
#define S_TYPEISTMO(buf) (0) /* Typed memory object */

int chmod(const char *path, mode_t mode);
int fchmod(int fd, mode_t mode);
int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags);

int fstat(int fd, struct stat *buf);
int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags);
int lstat(const char *path, struct stat *buf);
int stat(const char *path, struct stat *buf);

int futimens(int fd, const struct timespec times[2]);
int utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags);

int mkdir(const char *pathname, mode_t mode);
int mkdirat(int dirfd, const char *pathname, mode_t mode);

int mkfifo(const char *pathname, mode_t mode);
int mkfifoat(int dirfd, const char *pathname, mode_t mode);

/* XSI */
int mknod(const char *pathname, mode_t mode, dev_t dev);
int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev);

mode_t umask(mode_t mask);

#endif /* _SYS_STAT_H */