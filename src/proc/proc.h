#pragma once

#include <stdint.h>
#include <stddef.h>

#include "kernel.h"

#include "memory/mm_common.h"
#include "vfs/vfs.h"

#define PROCESS_FDS_MAX 64

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
    proc_t *parent;
    size_t pid;
    char name[PROCESS_THREAD_NAME_LEN];
    
    volatile proc_thread_state_t state;

    volatile thread_t * volatile threads;

    volatile pml4_t *pml4;

    // Make sure this is always less than SSIZE_MAX!
    size_t fds_max;
    vfs_handle_t **fds;

    volatile proc_t *first_child;
    volatile proc_t *prev_sibling;
    volatile proc_t *next_sibling;

    size_t uid;
    // TODO: implement groups
};

proc_t *create_proc(const char *name, proc_t *parent);
// void release_proc(proc_t *proc);
void process_exit(proc_t *proc);

proc_t *_get_proc_kernel();

vfs_handle_t *proc_get_fd(proc_t *proc, size_t fd);
ssize_t proc_alloc_fd(proc_t *proc, vfs_handle_t *handle);
void proc_free_fd(proc_t *proc, size_t fd);