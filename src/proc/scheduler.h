#pragma once

#include "proc/proc.h"
#include "proc/thread.h"

#include "cpu/cpu.h"

extern volatile thread_t * volatile cores_threads[MAX_CORES];

void thread_ready(thread_t *thread);
void thread_unready(thread_t *thread);

void assign_thread_to_cpu(thread_t *thread);

void cpu_scheduler_task();

uint32_t lock_ready_threads();

void unlock_ready_threads();

__attribute__((noreturn)) //
void start_scheduler();