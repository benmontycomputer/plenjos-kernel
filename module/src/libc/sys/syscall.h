#ifndef _LIBC_SYSCALL_H
#define _LIBC_SYSCALL_H 1

#include <stdint.h>
#include "types.h"

#include "plenjos/syscall.h"
#include "plenjos/dev/fb.h"
#include "plenjos/dev/kbd.h"

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi);

ssize_t syscall_open(const char *path, const char *mode);
ssize_t syscall_read(size_t fd, void *buf, size_t count);
ssize_t syscall_write(size_t fd, const void *buf, size_t count);
ssize_t syscall_lseek(size_t fd, ssize_t offset, int whence);

int syscall_close(size_t fd);

uint64_t syscall_get_fb(fb_info_t *out_fb_info);
kbd_buffer_state_t *syscall_get_kb();

int syscall_memmap(void *addr, size_t length);

void syscall_sleep(uint32_t ms);

#endif /* _LIBC_SYSCALL_H */