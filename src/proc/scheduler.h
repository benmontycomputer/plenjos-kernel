#pragma once

#include "cpu/cpu.h"
#include "proc/task.h"

typedef void (*kernel_task_func_t)(void *);

extern task_t *current[MAX_CORES];
extern task_t *init_task;

void kill_task(task_t *task);

void ipi_kill_routine(registers_t *regs, void *data);
void ipi_wakeup_routine(registers_t *regs, void *data);

// These DO NOT acquire the lock; it must be done by the caller
void task_ready(task_t *task);

// This DOES acquire the lock, as well as disabling interrupts
void task_ready_safe(task_t *task);

void lock_rq(void);
void unlock_rq(void);

__attribute__((noreturn)) //
extern void cpu_scheduler_task(void);

// DO NOT call this! It should only be called from assembly.
void cpu_scheduler_task_c(uint32_t curr_core);

// IMPORTANT: interrupts must remain DISABLED for the duration of this function, or its stack could get overwritten.
int delegate_kernel_task(kernel_task_func_t func, void *arg);

void populate_ktask(mm_t *mm);
void start_scheduler(void);
void stop_scheduler(void);

void block(wait_queue_t *wq, task_t *task);

void syscall_dispatch(registers_t *regs);