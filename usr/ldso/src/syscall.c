#include "syscall.h"

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

int syscall_memmap(void *addr, size_t length, syscall_memmap_flags_t flags) {
    return (int)syscall(SYSCALL_MEMMAP, (uint64_t)addr, (uint64_t)length, (uint64_t)flags, 0, 0);
}

int syscall_memmap_from_buffer(void *addr, size_t length, syscall_memmap_flags_t flags, void *buffer,
                               size_t buffer_length) {
    return (int)syscall(SYSCALL_MEMMAP_FROM_BUFFER, (uint64_t)addr, (uint64_t)length, (uint64_t)flags, (uint64_t)buffer,
                        (uint64_t)buffer_length);
}

int syscall_memprotect(void *addr, size_t length, syscall_memmap_flags_t flags) {
    return (int)syscall(SYSCALL_MEMPROTECT, (uint64_t)addr, (uint64_t)length, (uint64_t)flags, 0, 0);
}

int syscall_open(const char *path, syscall_open_flags_t flags, mode_t mode_if_create) {
    return (int)syscall(SYSCALL_OPEN, (uint64_t)path, (uint64_t)flags, (uint64_t)mode_if_create, 0, 0);
}

ssize_t syscall_read(int fd, void *buf, size_t count) {
    return (ssize_t)syscall(SYSCALL_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

int syscall_fstat(int fd, struct kstat *out_stat_ptr) {
    return (int)syscall(SYSCALL_FSTAT, (uint64_t)fd, (uint64_t)out_stat_ptr, 0, 0, 0);
}

int syscall_print(const char *str) {
    return (int)syscall(SYSCALL_PRINT, (uint64_t)str, 0, 0, 0, 0);
}

int syscall_print_ptr(uint64_t val) {
    return (int)syscall(SYSCALL_PRINT_PTR, (uint64_t)val, 0, 0, 0, 0);
}

int syscall_close(int fd) {
    return (int)syscall(SYSCALL_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}