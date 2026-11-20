#include "sys/syscall.h"

#include "plenjos/dev/fb.h"
#include "plenjos/dev/kbd.h"
#include "plenjos/syscall.h"
#include "plenjos/types.h"
#include "sys/types.h"

#include <stdint.h>

uint64_t syscall(uint64_t rax, uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t r8, uint64_t r9) {
    register uint64_t r_rdi asm("rdi") = rdi;
    register uint64_t r_rsi asm("rsi") = rsi;
    register uint64_t r_rdx asm("rdx") = rdx;
    register uint64_t r10_ asm("r10")  = r10;
    register uint64_t r8_ asm("r8")    = r8;
    register uint64_t r9_ asm("r9")    = r9;
    uint64_t ret;

    asm volatile("syscall"
                 : "=a"(ret)
                 : "a"(rax), "r"(r_rdi), "r"(r_rsi), "r"(r_rdx), "r"(r10_), "r"(r8_), "r"(r9_)
                 : "rcx", "r11", "memory");
    return ret;
}

int syscall_open(const char *path, syscall_open_flags_t flags, mode_t mode_if_create) {
    return (int)syscall(SYSCALL_OPEN, (uint64_t)path, (uint64_t)flags, (uint64_t)mode_if_create, 0, 0, 0);
}

ssize_t syscall_read(int fd, void *buf, size_t count) {
    return (ssize_t)syscall(SYSCALL_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0, 0);
}

ssize_t syscall_write(int fd, const void *buf, size_t count) {
    return (ssize_t)syscall(SYSCALL_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0, 0);
}

ssize_t syscall_getdents(int fd, struct plenjos_dirent *buf, size_t nbytes) {
    return (ssize_t)syscall(SYSCALL_GETDENTS, (uint64_t)fd, (uint64_t)buf, (uint64_t)nbytes, 0, 0, 0);
}

off_t syscall_lseek(int fd, off_t offset, int whence) {
    return (off_t)syscall(SYSCALL_LSEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence, 0, 0, 0);
}

int syscall_close(int fd) {
    return (int)syscall(SYSCALL_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
}

uint64_t syscall_get_fb(fb_info_t *out_fb_info) {
    return syscall(SYSCALL_GET_FB, (uint64_t)out_fb_info, 0, 0, 0, 0, 0);
}

int syscall_memmap(void *addr, size_t length, syscall_memmap_flags_t flags) {
    return (int)syscall(SYSCALL_MEMMAP, (uint64_t)addr, (uint64_t)length, (uint64_t)flags, 0, 0, 0);
}

int syscall_memmap_from_buffer(void *addr, size_t length, syscall_memmap_flags_t flags, void *buffer,
                               size_t buffer_length) {
    return (int)syscall(SYSCALL_MEMMAP_FROM_BUFFER, (uint64_t)addr, (uint64_t)length, (uint64_t)flags, (uint64_t)buffer,
                        (uint64_t)buffer_length, 0);
}

void syscall_sleep(uint32_t ms) {
    syscall(SYSCALL_SLEEP, (uint64_t)ms, 0, 0, 0, 0, 0);
}

int syscall_print(const char *str) {
    return (int)syscall(SYSCALL_PRINT, (uint64_t)str, 0, 0, 0, 0, 0);
}

int syscall_mkdir(const char *path, mode_t mode) {
    return (int)syscall(SYSCALL_MKDIR, (uint64_t)path, (uint64_t)mode, 0, 0, 0, 0);
}

int syscall_chdir(const char *path) {
    return (int)syscall(SYSCALL_CHDIR, (uint64_t)path, 0, 0, 0, 0, 0);
}

ssize_t syscall_getcwd(char *buf, ssize_t size) {
    return (ssize_t)syscall(SYSCALL_GETCWD, (uint64_t)buf, (uint64_t)size, 0, 0, 0, 0);
}

int syscall_mount(const char *dev, const char *target) {
    return (int)syscall(SYSCALL_MOUNT, (uint64_t)dev, (uint64_t)target, 0, 0, 0, 0);
}

pid_t syscall_spawn(const char *path, char *const argv[], char *const envp[], size_t argc, size_t envc) {
    return (pid_t)syscall(SYSCALL_SPAWN, (uint64_t)path, (uint64_t)argv, (uint64_t)envp, (uint64_t)argc,
                          (uint64_t)envc, 0);
}

_Noreturn void syscall_exit_thread() {
    syscall(SYSCALL_EXIT_THREAD, 0, 0, 0, 0, 0, 0);
}

_Noreturn void syscall_exit_process() {
    syscall(SYSCALL_EXIT_PROCESS, 0, 0, 0, 0, 0, 0);
}