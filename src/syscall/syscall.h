#pragma once

#include <stdint.h>

#include "kernel.h"

#include "memory/mm_common.h"

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi);
void syscalls_init();

// count must be <= SSIZE_MAX
ssize_t syscall_routine_read(size_t fd, void *buf, size_t count, pml4_t *current_pml4);
ssize_t syscall_routine_write(size_t fd, const void *buf, size_t count, pml4_t *current_pml4);
ssize_t syscall_routine_open(const char *path, uint64_t flags);
ssize_t syscall_routine_lseek(size_t fd, ssize_t offset, int whence);
ssize_t syscall_routine_getdents(size_t fd, void *buf, size_t count, pml4_t *current_pml4);
int syscall_routine_close(size_t fd);