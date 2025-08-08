#pragma once

#include "proc/proc.h"

#include <stdint.h>

#define THREAD_STACK_SIZE 0x4000 // 16 KiB

typedef struct thread thread_t;

struct thread {
    registers_t regs;

    size_t tid;
    char name[PROCESS_THREAD_NAME_LEN];

    proc_thread_state_t state;

    proc_t *parent;
    thread_t *next;

    void *stack;
};

thread_t *create_thread(proc_t *proc, const char *name, void (*func)(void *), void *arg);
void release_thread(thread_t *thread);