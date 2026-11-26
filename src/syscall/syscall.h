#pragma once

#include <stdint.h>

#include "kernel.h"

#include "memory/mm_common.h"
#include "proc/task.h"

#include "plenjos/stat.h"

void syscall(registers_t *regs, task_t *task);
void syscall_init_cpu(void);