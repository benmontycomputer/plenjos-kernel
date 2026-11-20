#pragma once

#include "cpu/cpu.h"
#include "proc/proc.h"
#include "proc/thread.h"

typedef void (*kernel_task_func_t)(void *);

extern thread_t *cores_threads[MAX_CORES];
extern proc_t *init_proc;

void kill_process_threads(proc_t *proc);

void ipi_kill_routine(registers_t *regs, void *data);
void ipi_wakeup_routine(registers_t *regs, void *data);

// These DO NOT acquire the lock; it must be done by the caller
void thread_ready(thread_t *thread);

// This DOES acquire the lock, as well as disabling interrupts
void thread_ready_safe(thread_t *thread);

void lock_thread_rq();
void unlock_thread_rq();

__attribute__((noreturn)) //
extern void cpu_scheduler_task();

// IMPORTANT: interrupts must remain DISABLED for the duration of this function, or its stack could get overwritten.
int delegate_kernel_task(kernel_task_func_t func, void *arg);

void start_scheduler();

void stop_scheduler();

void block(mutex *m, thread_t *thread);