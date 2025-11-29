#pragma once

struct fb_info {
    char *fb_ptr;
    int fb_scanline;
    int fb_width;
    int fb_height;
    int fb_bytes_per_pixel;
} __attribute__((packed));
typedef struct fb_info fb_info_t;

typedef enum {
    SYSCALL_READ,
    SYSCALL_WRITE,
    SYSCALL_OPEN,
    SYSCALL_CLOSE,
    SYSCALL_STAT,
    SYSCALL_FSTAT,
    SYSCALL_LSTAT,
    SYSCALL_POLL,
    SYSCALL_LSEEK,
    SYSCALL_MEMMAP,
    SYSCALL_GET_FB,
    SYSCALL_GET_KB,
    SYSCALL_PRINT,
    SYSCALL_PRINT_PTR,
    SYSCALL_KB_READ,
    SYSCALL_SLEEP,
} syscalls_call;