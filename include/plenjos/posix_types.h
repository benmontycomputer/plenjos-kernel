#ifndef _PLENJOS_POSIX_TYPES_H
#define _PLENJOS_POSIX_TYPES_H 1

#include <stdint.h>

typedef uint32_t __kernel_uid_t;
typedef uint32_t __kernel_gid_t;
typedef uint16_t __kernel_mode_t;

typedef __kernel_uid_t uid_t;
typedef __kernel_gid_t gid_t;
typedef __kernel_mode_t mode_t;

typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef ssize_t off_t;

typedef uint64_t __kernel_ino_t;
typedef __kernel_ino_t ino_t;

// blkcnt_t
typedef int64_t blkcnt_t;
// blksize_t
typedef int64_t blksize_t;
// clock_t
typedef int64_t clock_t;
// dev_t
typedef uint64_t dev_t;
// fsblkcnt_t
typedef uint64_t fsblkcnt_t;
// fsfilcnt_t
typedef uint64_t fsfilcnt_t;
// id_t
typedef uint32_t id_t;
// key_t
typedef int32_t key_t;
// nlink_t
typedef uint32_t nlink_t;
// pid_t
typedef int32_t pid_t;
// TODO: pthread_*_t and pthread_t
// reclen_t
typedef uint16_t reclen_t;
// suseconds_t
typedef int64_t suseconds_t;
// time_t
typedef int64_t time_t;
// timer_t
typedef int64_t timer_t;

#endif /* _PLENJOS_POSIX_TYPES_H */