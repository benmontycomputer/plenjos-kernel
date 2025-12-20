#pragma once

#include "proc/proc.h"

#include <stdint.h>

#define THREAD_STACK_SIZE 0x4000 // 16 KiB

typedef struct thread thread_t;

struct thread {
    volatile registers_t regs;

    pid_t tid;
    char name[PROCESS_THREAD_NAME_LEN];

    volatile proc_thread_state_t state;

    proc_t *parent;
    volatile thread_t * volatile next;

    gsbase_t *base;

    void *stack;
} __attribute__((packed));

thread_t *create_thread(proc_t *proc, const char *name, void (*func)(void *), void *arg);
void release_thread(thread_t *thread);