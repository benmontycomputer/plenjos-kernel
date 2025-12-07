#ifndef _PLENJOS_TYPES_H
#define _PLENJOS_TYPES_H 1

#include <stdint.h>

typedef uint32_t __kernel_uid_t;
typedef uint32_t __kernel_gid_t;
typedef uint16_t __kernel_mode_t;

typedef __kernel_uid_t uid_t;
typedef __kernel_gid_t gid_t;
typedef __kernel_mode_t mode_t;

typedef int64_t ssize_t;

#endif /* _PLENJOS_TYPES_H */