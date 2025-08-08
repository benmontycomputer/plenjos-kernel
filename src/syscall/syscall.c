#include <stdint.h>

#include "syscall/syscall.h"

#include "kernel.h"

#include "arch/x86_64/irq.h"

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi, uint64_t ebp) {
    uint64_t out;

    asm volatile (
        "mov %[_rax], %%rax\n"
        "mov %[_rbx], %%rbx\n"
        "mov %[_rcx], %%rcx\n"
        "mov %[_rdx], %%rdx\n"
        "mov %[_rsi], %%rsi\n"
        "mov %[_rdi], %%rdi\n"
        "mov %[_ebp], %%ebp\n"
        "int $0x80\n"
        "mov %%rax, %[_out]\n"
        : [_out] "=r" (out)
        : [_rax] "r" (rax), [_rbx] "r" (rbx), [_rcx] "r" (rcx), [_rdx] "r" (rdx), [_rsi] "r" (rsi), [_rdi] "r" (rdi), [_ebp] "r" (ebp)
        : "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "ebp"   // Clobbered registers
    );

    return out;
}

void syscall_routine(registers_t *regs) {
    uint64_t call = regs->rax;
}

void syscalls_init() {
    irq_register_routine(128, syscall_routine);
}