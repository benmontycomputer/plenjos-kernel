#ifndef _PLENJOS_ERRNO_H
#define _PLENJOS_ERRNO_H 1

/** This file contains various error code definitions used across the kernel, primarily for syscalls.
 *
 * When these error codes are returned from syscalls, they are returned as -1 * (errno). However, in userspace (libc),
 * errno is always positive instead (not including syscall routines themselves obviously).
 */

// Required by C standard
#define EDOM   39 // Device not managed
#define ERANGE 34 // Result too large
#define EILSEQ 42 // Illegal byte sequence

// General errors
#define ENOMEM 12 // Out of memory
#define EFAULT 14 // Bad address
#define EINVAL 22 // Invalid argument
#define ENOSYS 38 // Function not implemented

// I/O errors
#define ENOENT       2  // No such file or directory
#define EIO          5  // I/O error
#define EBADF        9  // Bad file descriptor
#define EACCES       13 // Permission denied
#define EEXIST       17 // File exists
#define ENOTDIR      20 // Not a directory
#define EISDIR       21 // Is a directory
#define EMFILE       24 // Too many open files
#define ENAMETOOLONG 36 // Filename too long

#endif /* _PLENJOS_ERRNO_H */