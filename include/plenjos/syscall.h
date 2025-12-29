#ifndef _PLENJOS_SYSCALL_H
#define _PLENJOS_SYSCALL_H 1

#include <stdint.h>

#define SYSCALL_FS_FIRST 0
#define SYSCALL_FS_LAST 0x3F

typedef enum {
    // Filesystem
    SYSCALL_READ,
    SYSCALL_WRITE,
    SYSCALL_OPEN,
    SYSCALL_CLOSE,
    SYSCALL_STAT,
    SYSCALL_FSTAT,
    SYSCALL_LSTAT,
    SYSCALL_POLL,
    SYSCALL_LSEEK,
    SYSCALL_GETDENTS,
    SYSCALL_MKDIR,
    SYSCALL_RMDIR,
    SYSCALL_RENAME,
    SYSCALL_CHMOD,
    SYSCALL_FCHMOD,
    SYSCALL_CHOWN,
    SYSCALL_FCHOWN,
    SYSCALL_LCHOWN,
    // TODO: modify file's groups
    SYSCALL_GETCWD,
    SYSCALL_CHDIR,
    SYSCALL_FCHDIR,
    SYSCALL_LINK,
    SYSCALL_UNLINK,
    SYSCALL_SYMLINK,
    SYSCALL_READLINK,

    // Other
    SYSCALL_MEMMAP = 0x40,
    SYSCALL_MEMMAP_FROM_BUFFER, // Used for read-only mappings; this might be temporary. These should be protected from unmapping.
    SYSCALL_MEMMAP_FILE,
    SYSCALL_MEMPROTECT, // Currently only supports removing permissions, not adding them.
    SYSCALL_ALLOC_PAGE,
    SYSCALL_GET_FB,
    SYSCALL_GET_KB,
    SYSCALL_PRINT,
    SYSCALL_PRINT_PTR,
    SYSCALL_KB_READ,
    SYSCALL_SLEEP,
} syscalls_call;

typedef uint8_t syscall_open_flags_t;

#define SYSCALL_OPEN_FLAG_EX 0x1
#define SYSCALL_OPEN_FLAG_WRITE 0x2
#define SYSCALL_OPEN_FLAG_READ 0x4
#define SYSCALL_OPEN_FLAG_CREATE 0x8
#define SYSCALL_OPEN_FLAG_EXCL 0x10
#define SYSCALL_OPEN_FLAG_DIRECTORY 0x20

typedef uint8_t syscall_memmap_flags_t;

// #define SYSCALL_MEMMAP_FLAG_RD 0x1 // Read is implied
#define SYSCALL_MEMMAP_FLAG_WR 0x1
#define SYSCALL_MEMMAP_FLAG_EX 0x2

#endif /* _PLENJOS_SYSCALL_H */