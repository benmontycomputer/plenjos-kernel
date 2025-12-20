// POSIX
// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/fcntl.h.html

#ifndef _FCNTL_H
#define _FCNTL_H 1

#include "sys/types.h" /* off_t, pid_t, mode_t */
#include "unistd.h"

/**
 * File access modes (exactly one required)
 */
#define O_RDONLY  0x0000
#define O_WRONLY  0x0001
#define O_RDWR    0x0002
#define O_EXEC    0x0003
#define O_SEARCH  0x0004
#define O_ACCMODE 0x0007 // Mask for file access modes

/**
 * File creation flags; used in open() and openat()
 */
#define O_CREAT     0x0010 // Create file if it does not exist
#define O_DIRECTORY 0x0020 // Fail if not a directory
#define O_EXCL      0x0040 // Exclusive use flag
#define O_NOCTTY    0x0080 // Do not assign controlling terminal
#define O_TRUNC     0x0100 // Truncate file to zero length
#define O_CLOEXEC   0x0200 // Atomically set FD_CLOEXEC on the new file descriptor
#define O_CLOFORK   0x0400 // Atomically set FD_CLOFORK on the new file descriptor
#define O_NOFOLLOW  0x0800 // Do not follow symbolic links
#define O_TTY_INIT  0x1000 // Set the termios structure technical parameters; see docs for details

/**
 * File status flags; used in open(), openat(), and fcntl()
 */
#define O_APPEND   0x00008 // Writes append to the end of the file
#define O_NONBLOCK 0x02000 // Non-blocking mode
#define O_SYNC     0x04000 // Writes complete as defined by synchronized I/O file integrity completion
#define O_DSYNC    0x08000 // Writes complete as defined by synchronized I/O data integrity completion
#define O_RSYNC    0x10000 // Writes complete as defined by synchronized I/O file integrity completion

/**
 * Commands for fcntl()
 */
#define F_DUPFD         0  // Duplicate file descriptor
#define F_DUPFD_CLOEXEC 1  // Duplicate file descriptor; set close-on-exec flag
#define F_DUPFD_CLOFORK 2  // Duplicate file descriptor; set close-on-fork flag
#define F_GETFD         3  // Get file descriptor flags
#define F_SETFD         4  // Set file descriptor flags
#define F_GETFL         5  // Get file status flags
#define F_SETFL         6  // Set file status flags
#define F_GETLK         7  // Get record locking information
#define F_SETLK         8  // Set record locking information
#define F_SETLKW        9  // Set record locking information; wait if blocked
#define F_OFD_GETLK     10 // Get OFD record locking information
#define F_OFD_SETLK     11 // Set OFD record locking information
#define F_OFD_SETLKW    12 // Set OFD record locking information; wait if blocked
#define F_GETOWN        13 // Get owner (Linux-specific)
#define F_GETOWN_EX     14 // Get owner with type (Linux-specific)
#define F_SETOWN        15 // Set owner (Linux-specific)
#define F_SETOWN_EX     16 // Set owner with type (Linux-specific)

/**
 * File descriptor flags
 */
#define FD_CLOEXEC 0x1 // Close on exec
#define FD_CLOFORK 0x2 // Close on fork

/**
 * Record locking
 */
#define F_RDLCK 1 // Shared or read lock
#define F_WRLCK 2 // Exclusive or write lock
#define F_UNLCK 3 // Unlock

/**
 * For pid member of f_owner_ex structure
 */
#define F_OWNER_PID  1 // Process ID
#define F_OWNER_PGRP 2 // Process group ID

/**
 * Symbolic constants used in place of file descriptors for the *at() functions
 */
#define AT_FDCWD -100 // Use the current working directory to determine the target of relative paths

/**
 * The following 4 flags, while they share a prefix, each apply to a distinct set of functions:
 *   AT_EACCES: faccesat()
 *   AT_SYMLINK_NOFOLLOW: fstatat(), fchmodat(), fchownat(), and utimensat()
 *   AT_SYMLINK_FOLLOW: linkat()
 *   AT_REMOVEDIR: unlinkat()
 */
#define AT_EACCESS          0x1 // Check access using effective user and group ID
#define AT_SYMLINK_NOFOLLOW 0x1 // Do not follow symbolic links
#define AT_SYMLINK_FOLLOW   0x1 // Follow symbolic links
#define AT_REMOVEDIR        0x1 // Remove directory instead of file

/**
 * Values for the advice parameter of posix_fadvise()
 */
#define POSIX_FADV_NORMAL     0 // No special advice
#define POSIX_FADV_SEQUENTIAL 1 // Expect sequential page references
#define POSIX_FADV_RANDOM     2 // Expect random page references
#define POSIX_FADV_NOREUSE    3 // Expect access only once
#define POSIX_FADV_WILLNEED   4 // Expect access in the near future
#define POSIX_FADV_DONTNEED   5 // Do not expect access in the near future

int creat(const char *pathname, mode_t mode);
int open(const char *pathname, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...);
int fcntl(int fd, int cmd, ...);
int posix_fadvise(int fd, off_t offset, off_t len, int advice);
int posix_fallocate(int fd, off_t offset, off_t len);

#endif /* _FCNTL_H */