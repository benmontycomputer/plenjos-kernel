#pragma once

#include <stdint.h>

#include "kernel.h"

#include "memory/mm_common.h"
#include "proc/proc.h"

#include "plenjos/stat.h"

void syscall(registers_t *regs, thread_t *thread, pml4_t *current_pml4);
void syscall_init_cpu();