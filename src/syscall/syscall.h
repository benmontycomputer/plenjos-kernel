#pragma once

#include <stdint.h>
#include <sys/types.h>

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

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi);
void syscalls_init();

// count must be <= SSIZE_MAX
ssize_t syscall_routine_read(size_t fd, void *buf, size_t count);
ssize_t syscall_routine_write(size_t fd, const void *buf, size_t count);
size_t syscall_routine_open(const char *path, const char *mode);
int syscall_routine_close(size_t fd);