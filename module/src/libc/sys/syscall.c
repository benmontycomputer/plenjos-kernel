#include "syscall.h"

#include <stdint.h>

#include "types.h"

#include "plenjos/syscall.h"

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

ssize_t syscall_open(const char *path, syscall_open_flags_t flags) {
    return (ssize_t)syscall(SYSCALL_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0);
}

ssize_t syscall_read(size_t fd, void *buf, size_t count) {
    return (ssize_t)syscall(SYSCALL_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

int syscall_close(size_t fd) {
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