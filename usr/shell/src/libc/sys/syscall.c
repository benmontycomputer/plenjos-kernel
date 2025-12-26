#include "syscall.h"

#include <stdint.h>

#include "types.h"

#include "plenjos/syscall.h"
#include "plenjos/types.h"

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi) {
    uint64_t out;

    asm volatile("mov %[_rax], %%rax\n"
                 "mov %[_rbx], %%rbx\n"
                 "mov %[_rcx], %%rcx\n"
                 "mov %[_rdx], %%rdx\n"
                 "mov %[_rsi], %%rsi\n"
                 "mov %[_rdi], %%rdi\n"
                 "int $0x80\n"
                 "mov %%rax, %[_out]\n"
                 : [_out] "=r"(out)
                 : [_rax] "r"(rax), [_rbx] "r"(rbx), [_rcx] "r"(rcx), [_rdx] "r"(rdx), [_rsi] "r"(rsi), [_rdi] "r"(rdi)
                 : "rax", "rbx", "rcx", "rdx", "rsi", "rdi" // Clobbered registers
    );

    return out;
}

int syscall_open(const char *path, syscall_open_flags_t flags, mode_t mode_if_create) {
    return (int)syscall(SYSCALL_OPEN, (uint64_t)path, (uint64_t)flags, (uint64_t)mode_if_create, 0, 0);
}

ssize_t syscall_read(int fd, void *buf, size_t count) {
    return (ssize_t)syscall(SYSCALL_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

ssize_t syscall_write(int fd, const void *buf, size_t count) {
    return (ssize_t)syscall(SYSCALL_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

ssize_t syscall_getdents(int fd, struct plenjos_dirent *buf, size_t nbytes) {
    return (ssize_t)syscall(SYSCALL_GETDENTS, (uint64_t)fd, (uint64_t)buf, (uint64_t)nbytes, 0, 0);
}

off_t syscall_lseek(int fd, off_t offset, int whence) {
    return (off_t)syscall(SYSCALL_LSEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence, 0, 0);
}

int syscall_close(int fd) {
    return (int)syscall(SYSCALL_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

uint64_t syscall_get_fb(fb_info_t *out_fb_info) {
    return syscall(SYSCALL_GET_FB, (uint64_t)out_fb_info, 0, 0, 0, 0);
}

kbd_buffer_state_t *syscall_get_kb() {
    return (kbd_buffer_state_t *)syscall(SYSCALL_GET_KB, 0, 0, 0, 0, 0);
}

int syscall_memmap(void *addr, size_t length) {
    return (int)syscall(SYSCALL_MEMMAP, (uint64_t)addr, (uint64_t)length, 0, 0, 0);
}

void syscall_sleep(uint32_t ms) {
    syscall(SYSCALL_SLEEP, (uint64_t)ms, 0, 0, 0, 0);
}

int syscall_print(const char *str) {
    return (int)syscall(SYSCALL_PRINT, (uint64_t)str, 0, 0, 0, 0);
}

int syscall_mkdir(const char *path, mode_t mode) {
    return (int)syscall(SYSCALL_MKDIR, (uint64_t)path, (uint64_t)mode, 0, 0, 0);
}

int syscall_chdir(const char *path) {
    return (int)syscall(SYSCALL_CHDIR, (uint64_t)path, 0, 0, 0, 0);
}

ssize_t syscall_getcwd(char *buf, ssize_t size) {
    return (ssize_t)syscall(SYSCALL_GETCWD, (uint64_t)buf, (uint64_t)size, 0, 0, 0);
}