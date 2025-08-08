#pragma once

#include <stdint.h>
#include <stddef.h>

#include "kernel.h"

#include "memory/mm_common.h"

struct thread;
typedef struct thread thread_t;

#define PROCESS_THREAD_NAME_LEN 64

typedef enum {
    READY,
    ASLEEP,
    RUNNING,
    DEAD,
} proc_thread_state_t;

typedef struct proc proc_t;

struct proc {
    size_t pid;
    char name[PROCESS_THREAD_NAME_LEN];
    
    proc_thread_state_t state;

    thread_t *threads;

    pml4_t *pml4;

    proc_t *next;
};

proc_t *create_proc(const char *name);
void release_proc(proc_t *proc);