#include <stdint.h>
#include <stdbool.h>

#include "proc/scheduler.h"

#include "proc/proc.h"
#include "proc/thread.h"

#include "cpu/cpu.h"

thread_t *ready_threads = NULL;
thread_t *ready_threads_last = NULL;

thread_t *cores_threads[MAX_CORES] = {0};

static bool ready_threads_lock = false;

static void lock_ready_threads() {
    for (;;) {
        if (!ready_threads_lock) {
            ready_threads_lock = true;
            return;
        }
    }
}

static void unlock_ready_threads() {
    ready_threads_lock = false;
}

void thread_ready(thread_t *thread) {
    lock_ready_threads();

    if (!ready_threads) {
        ready_threads = thread;
        ready_threads_last = thread;
    } else {
        ready_threads_last->next = thread;
        ready_threads_last = thread;
    }

    unlock_ready_threads();
}

void thread_unready(thread_t *thread) {
    lock_ready_threads();

    if (ready_threads == thread) {
        ready_threads = thread->next;
    }

    unlock_ready_threads();
}

extern void restore_cpu_state(registers_t *regs);

void assign_thread_to_cpu(thread_t *thread) {
    restore_cpu_state(&thread->regs);
}