#pragma once

#include <stdint.h>

#include "kernel.h"

#include "memory/mm_common.h"
#include "proc/proc.h"

#include "plenjos/stat.h"

uint64_t syscall(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi, uint64_t rdi);
void syscalls_init();