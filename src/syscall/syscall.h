#pragma once

#include <stdint.h>

typedef enum {
    SYSCALL_GET_FB,
    SYSCALL_GET_KB,
    SYSCALL_PRINT,
    SYSCALL_READ,
} syscalls_call;

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi);
void syscalls_init();