#ifndef _LIBC_SYSCALL_H
#define _LIBC_SYSCALL_H 1

#include "plenjos/dev/fb.h"
#include "plenjos/dev/kbd.h"
#include "plenjos/dirent.h"
#include "plenjos/syscall.h"
#include "plenjos/types.h"
#include "types.h"

#include <stdint.h>

uint64_t syscall(uint64_t rax, uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t r8, uint64_t r9);

int syscall_open(const char *path, syscall_open_flags_t flags, mode_t mode_if_create);
ssize_t syscall_read(int fd, void *buf, size_t count);
ssize_t syscall_write(int fd, const void *buf, size_t count);
off_t syscall_lseek(int fd, off_t offset, int whence);

ssize_t syscall_getdents(int fd, struct plenjos_dirent *buf, size_t nbytes);

int syscall_close(int fd);

uint64_t syscall_get_fb(fb_info_t *out_fb_info);

void *syscall_memmap(void *hint, size_t length, syscall_memmap_flags_t flags);
void *syscall_memmap_from_buffer(void *hint, size_t length, syscall_memmap_flags_t flags, void *buffer,
                               size_t buffer_length);

void syscall_sleep(uint32_t ms);

int syscall_print(const char *str);

int syscall_mkdir(const char *path, mode_t mode);

int syscall_chdir(const char *path);

// Yes, the size parameter should be ssize_t
ssize_t syscall_getcwd(char *buf, ssize_t size);

int syscall_mount(const char *dev, const char *target);

pid_t syscall_spawn(const char *path, char *const argv[], char *const envp[], size_t argc, size_t envc);

int syscall_waittid(pid_t tid);

_Noreturn void syscall_exit(void);
_Noreturn void syscall_exit_group(void);

#endif /* _LIBC_SYSCALL_H */