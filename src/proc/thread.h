#pragma once

#include "proc/proc.h"

#include <stdint.h>

// TODO: use page faults to potentially map more pages on stack overflow?
// TODO: do we want this to be smaller? This is pretty large.
#define THREAD_STACK_SIZE     0x100000            // 1 MiB
#define THREAD_MAX_STACK_SIZE 0x10000000 - 0x1000 // 256 MiB - 4 KiB guard page

typedef struct thread thread_t;

struct thread {
    volatile registers_t regs;

    pid_t tid;
    pid_t index_in_parent;
    char name[PROCESS_THREAD_NAME_LEN];

    volatile proc_thread_state_t state;

    proc_t *parent;
    volatile thread_t *volatile next;

    gsbase_t *base;

    // Always points to the top of the allocation for the thread's stack; this does NOT change on push/pop (look at
    // registers for that). This is not a valid memory address; this is directly on top of the stack, not inside its
    // allocation.
    void *stack_top;
} __attribute__((packed));

thread_t *create_thread(proc_t *proc, const char *name, void (*func)(void *), void *arg);
void release_thread(thread_t *thread);