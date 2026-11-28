#pragma once

#include "kernel.h"
#include "lib/structures/rbtree.h"
#include "lib/sync.h"
#include "memory/mm_common.h"
#include "vfs/vfs.h"

#include <stddef.h>
#include <stdint.h>

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

// TODO: cloexec
typedef struct task_handles {
    rwsem_t lock;

    // Make sure this is always less than SSIZE_MAX!
    int fds_max;
    vfs_handle_t **fds;

    atomic_int ref_cnt;
} task_handles_t;

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

    volatile task_state_t state; // Scheduler use

    // For kernel-only tasks, this is NULL
    mm_t *mm;
    // For kernel-only tasks, this stores the mm of whichever task, user or kernel, ran before it, as all tasks have the
    // same mappings for the kernel half of memory, so keeping the old mm avoids a cr3 rewrite. For a task that might
    // run in user mode, this is the same as mm.
    mm_t *mm_to_use;

    task_handles_t *handles;

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

task_t *create_task(const char *name, task_t *parent, mm_t *mm, task_handles_t *handles);
void task_exit(task_t *task);

int waittid(task_t *task, pid_t tid);

/* task_handles.c */
task_handles_t *task_handles_new();
vfs_handle_t *task_handles_get_fd(task_handles_t *handles, int fd);
int task_handles_alloc_fd(task_handles_t *handles, vfs_handle_t *handle);
int task_handles_free_fd(task_handles_t *handles, int fd);
int task_handles_ref(task_handles_t *handles);
int task_handles_unref(task_handles_t *handles);

/**
 * If argc == 0 or argv == NULL, argv is just { path }.
 *
 * If task is the current task, regs must be its saved user registers; on success they are rewritten so that returning
 * from the syscall enters the new program. Otherwise, regs is ignored and task is made ready to run.
 */
int execute(const char *path, task_t *task, int argc, char *const *argv, int envc, char *const *envp,
            registers_t *regs);