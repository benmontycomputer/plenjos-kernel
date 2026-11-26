#pragma once

#include "kernel.h"
#include "lib/structures/rbtree.h"
#include "lib/sync.h"
#include "memory/mm_common.h"
#include "vfs/vfs.h"

#include <stddef.h>
#include <stdint.h>

#define FDS_MAX 64

#define TASK_STATUS_EXITED      0x100
#define TASK_STATUS_MASK_RESULT 0xFF

struct thread;
typedef struct thread thread_t;

#define TASK_NAME_LEN 64

typedef enum {
    READY,
    BLOCKED,
    RUNNING,
    DEAD,
} task_state_t;

// These are page-aligned; remember to use phys_mem_unref_frame on both the process and its cwd when destroying a
// process. kfree_heap() will NOT work and could break things!
typedef struct task task_t;

// NOTE: unless a task is running (in which case it can't be restored), it will ALWAYS restore into kernel mode.
struct task {
    rbnode_t rbnode;

    pid_t tid;  // returned by gettid()
    pid_t tgid; // returned by getpid()
    char name[TASK_NAME_LEN];

    task_t *parent; // the task responsible for creating and waiting on this task
    task_t *children;
    task_t *sibling;
    task_t *group_leader; // points to this task if this task is group leader

    task_t *q_next; // for rq, wq, etc.

    wait_queue_t *blocked_by;

    void *kstack_base; // Points to BOTTOM
    void *kstack_top;

    volatile task_state_t state;

    // For kernel-only tasks, this is NULL
    mm_t *mm;
    // For kernel-only tasks, this stores the mm of whichever task, user or kernel, ran before it, as all tasks have the
    // same mappings for the kernel half of memory, so keeping the old mm avoids a cr3 rewrite. For a task that might
    // run in user mode, this is the same as mm.
    mm_t *mm_to_use;

    // Make sure this is always less than SSIZE_MAX!
    int fds_max;
    vfs_handle_t **fds;

    _Atomic(pid_t) next_thread_index;

    uid_t uid;
    char *cwd; // Page-aligned; 4096 bytes

    wait_queue_t exit_wq;
    uint32_t status;

    // TODO: should cwd be a file descriptor instead?
    // TODO: implement groups

    struct {
        uint64_t rsp; // Saved kernel stack pointer

        uint16_t es;
        uint16_t ds;
        uint16_t fs;
        uint16_t gs;

        uint64_t fsbase;
        uint64_t gsbase;

        uint64_t cr2; // for page faults
        uint64_t int_no;
        uint64_t err_code;

        // TODO: when FPU is enabled, those would go here
    } thread_state;
};

task_t *create_task(const char *name, task_t *parent);
void task_exit(task_t *task);

vfs_handle_t *task_get_fd(task_t *task, int fd);
int task_alloc_fd(task_t *task, vfs_handle_t *handle);
void task_free_fd(task_t *task, int fd);

int execute(const char *path, task_t *task);