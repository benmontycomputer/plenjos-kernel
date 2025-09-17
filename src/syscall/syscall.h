#pragma once

#include <stdint.h>

struct fb_info {
    char *fb_ptr;
    int fb_scanline;
    int fb_width;
    int fb_height;
    int fb_bytes_per_pixel;
} __attribute__((packed));
typedef struct fb_info fb_info_t;

typedef enum {
    SYSCALL_NONE,
    SYSCALL_GET_FB,
    SYSCALL_GET_KB,
    SYSCALL_MEMMAP,
    SYSCALL_PRINT,
    SYSCALL_PRINT_PTR,
    SYSCALL_READ,
    SYSCALL_SLEEP,
} syscalls_call;

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi);
void syscalls_init();