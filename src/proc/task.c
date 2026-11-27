#include "proc/task.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/common.h"
#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/gdt/tss.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"
#include "cpu/cpu.h"
#include "exec/elf.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/structures/rbtree.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "proc/scheduler.h"
#include "syscall/syscall_helpers.h"
#include "vfs/fscache.h"

#include <stdint.h>

extern char __kernel_start[];
extern char __kernel_end[];

// Largest executable (or dynamic linker) we are willing to read into memory
#define EXEC_MAX_FILE_SIZE (64ULL << 20)
// Longest path we'll copy onto the new task's stack (it has to fit in the top stack page with the argv block)
#define EXEC_MAX_PATH      2048

#define DEFAULT_FLAGS 0x202 // Enable interrupts

volatile pid_t next_tid = 1;

static fixed_size_allocator_t _task_fsa = FSA_DEFAULT(sizeof(task_t));

static int tid_cmp(const rbnode_t *a, const rbnode_t *b) {
    task_t *ta = (task_t *)a;
    task_t *tb = (task_t *)b;

    return (ta->tid > tb->tid) - (ta->tid < tb->tid);
}

static rbtree_t tid_tree = (rbtree_t) { 0 };

// TODO: convert this to rwlock
mutex tasklist_locked = MUTEX_INIT;
bool tasklist_int     = false;

// Lock and unlock tasklist DO handle saving/restoring IRQs
static void lock_tasklist() {
    bool _i = irq_save();
    mutex_lock(&tasklist_locked);
    tasklist_int = _i;
}

static void unlock_tasklist() {
    mutex_unlock(&tasklist_locked);
    irq_restore(tasklist_int);
}

task_t *create_task(const char *name, task_t *parent) {
    if (!tid_tree.cmp) {
        rbtree_init(&tid_tree, tid_cmp);
    }

    if (!parent) {
        if (next_tid != 1) {
            printf("Error: trying to create a new task (name %s), but tid 1 is taken and no parent was provided!\n",
                   name);
            return NULL;
        }
    }

    task_t *task = (task_t *)fsa_alloc(&_task_fsa);
    if (!task) {
        kout(KERNEL_WARN, "create_task(): fsa_alloc() returned NULL (OOM?)\n");
        return NULL;
    }

    uint64_t cwd_frame = find_next_free_frame();
    if (!cwd_frame) {
        kout(KERNEL_WARN, "Error allocating memory for task cwd (OOM?).\n");
        fsa_free(&_task_fsa, task);
        return NULL;
    }
    char *cwd = (char *)phys_to_virt(cwd_frame);
    memset(cwd, 0, PAGE_LEN);
    task->cwd = cwd;

    if (name) {
        strncpy(task->name, name, TASK_NAME_LEN);
        task->name[TASK_NAME_LEN - 1] = '\0';
    } else {
        task->name[0] = 0;
    }

    task->children = NULL;
    task->sibling  = NULL;
    task->parent   = parent;

    if (parent) {
        task->uid = parent->uid;
        strncpy(task->cwd, parent->cwd, PATH_MAX);
        task->cwd[PATH_MAX - 1] = '\0';
    } else {
        task->uid = 0;
        strncpy(task->cwd, "/", PATH_MAX);
        task->cwd[PATH_MAX - 1] = '\0';
    }

    task->state   = BLOCKED;
    task->exit_wq = WQ_INIT;

    mm_t *mm = mm_new();
    if (!mm) {
        kout(KERNEL_WARN, "Error allocating memory manager for task %s (OOM?)\n", task->name);
        phys_mem_unref_frame(phys_addr_to_frame_struct(cwd_frame));
        fsa_free(&_task_fsa, task);
        return NULL;
    }

    task->mm        = mm;
    task->mm_to_use = mm;

    task->fds_max = FDS_MAX;

    size_t fds_size = sizeof(vfs_handle_t *) * task->fds_max;

    task->fds = kmalloc_heap(fds_size);
    if (!task->fds) {
        kout(KERNEL_WARN, "Error allocating fd table for task %s (OOM?)\n", task->name);
        mm_unref(task->mm);
        phys_mem_unref_frame(phys_addr_to_frame_struct(cwd_frame));
        fsa_free(&_task_fsa, task);
        return NULL;
    }
    memset((void *)task->fds, 0, fds_size);

    task->tid = __atomic_fetch_add(&next_tid, 1, __ATOMIC_SEQ_CST);

    task->kstack_top = (void *)alloc_kstack();

    lock_tasklist();
    if (parent) {
        // Insert this task into the list
        if (parent->children) {
            task->sibling = parent->children;
        }

        parent->children = task;
    }

    rbtree_insert(&tid_tree, &task->rbnode);
    unlock_tasklist();

    return task;
}

void task_exit(task_t *task) {
    if (!task) return;

    if (task->tid <= 1) {
        kout(KERNEL_PANIC, "\n\n[ FATAL ERROR: task_exit called on tid %d! ]\n", (int)task->tid);
    }

    // Kill all children; tasklist does NOT need to be locked
    while (task->children) {
        // TODO: reparent instead? (POSIX)
        task_exit(task->children);
    }

    kill_task(task);

    lock_tasklist();
    task->status |= TASK_STATUS_EXITED;
    unlock_tasklist();

    for (int i = 0; i < task->fds_max; i++) {
        if (task->fds[i]) {
            vfs_close(task->fds[i]);
        }
    }
    kfree_heap(task->fds);
    task->fds = NULL;

    phys_mem_unref_frame(phys_addr_to_frame_struct(virt_to_phys((uint64_t)task->cwd)));

    mm_unref(task->mm);
    wq_signal(&task->exit_wq, task->status |= TASK_STATUS_EXITED, true);
}

static void task_reap_finish(task_t *task) {
    lock_tasklist();
    task_t **link = task->parent ? &task->parent->children : NULL;

    while (*link && *link != task) {
        link = &((*link)->sibling);
    }

    if (!*link) {
        kout(KERNEL_PANIC, "ERROR: when destroying task %d (%s), we couldn't find it in it's parent's tasks!\n",
             (int)task->tid, task->name);
    } else {
        *link = task->sibling;
    }

    rbtree_remove(&tid_tree, &task->rbnode);
    unlock_tasklist();

    fsa_free(&_task_fsa, task);
}

int waittid(task_t *task, pid_t tid) {
    lock_tasklist();

    task_t *c = task->children;

    while (c) {
        if (c->tid == tid) {
            wq_wait_event_update(&c->exit_wq, c->status & TASK_STATUS_EXITED, unlock_tasklist(), );
            int res = c->status & TASK_STATUS_MASK_RESULT;
            task_reap_finish(c);
            return res;
        }

        c = c->sibling;
    }

    unlock_tasklist();
    return -ECHILD;
}

vfs_handle_t *task_get_fd(task_t *task, int fd) {
    if (!task) {
        return NULL;
    }

    if (fd < 0) {
        printf("task_get_fd: fd %d out of range (negative) for task %s (tid %d)\n", fd, task->name, (int)task->tid);
        return NULL;
    }

    if (fd >= task->fds_max) {
        printf("task_get_fd: fd %d out of range (too high) for task %s (tid %d)\n", fd, task->name, (int)task->tid);
        return NULL;
    }

    return task->fds[fd];
}

int task_alloc_fd(task_t *task, vfs_handle_t *handle) {
    if (!handle) {
        printf("task_alloc_fd: handle is NULL\n");
        return -1;
    }

    for (int i = 0; i < task->fds_max; i++) {
        if (task->fds[i] == NULL) {
            task->fds[i] = handle;
            return i;
        }
    }

    return -1;
}

// WARNING: this does *not* free the underlying vfs_handle_t!
void task_free_fd(task_t *task, int fd) {
    if (fd >= task->fds_max) {
        return;
    }

    task->fds[fd] = NULL;
}

void *linker_elf_buf   = NULL;
size_t linker_elf_size = 0;

static void load_linker() {
    vfs_handle_t *ld_handle = NULL;
    int res                 = vfs_open("/lib/ld.so", SYSCALL_OPEN_FLAG_READ, 0, 0, &ld_handle);
    ssize_t linker_read_bytes;
    if (res == 0) {
        kout(KERNEL_INFO, "Loading ld.so...\n\n");
        uint64_t file_size = ld_handle->backing_node->stat.size;

        linker_elf_buf = kmalloc_heap(file_size);
        if (!linker_elf_buf) {
            panic("OOM error allocating memory for elf buffer\n");
        }
        linker_elf_size = file_size;

        linker_read_bytes = vfs_read(ld_handle, linker_elf_buf, file_size);
        vfs_close(ld_handle);
        if (linker_read_bytes < 0) {
            panic("Error reading /lib/ld.so: %d\n", (int)linker_read_bytes);
        } else if ((uint64_t)linker_read_bytes != file_size) {
            panic("Short read reading /lib/ld.so: expected %p, got %p\n", (void *)file_size, (void *)linker_read_bytes);
        }
    } else {
        panic("Error opening /lib/ld.so: %d\n", res);
    }
}

static void stack_push_qword(uint64_t qword, uint64_t **rsp) {
    *rsp  = (uint64_t *)((uint8_t *)(*rsp) - sizeof(uint64_t));
    **rsp = qword;
}

extern void user_task_hook(void);

#define EXEC_ALIGN8(x) (((x) + 7ULL) & ~7ULL)

// Total bytes the arguments take on the (single-page) initial user stack, or 0 if they're invalid/don't fit.
static size_t exec_args_stack_size(int argc, char *const *argv, int envc, char *const *envp) {
    size_t strings = 0;

    for (int i = 0; i < argc; i++) {
        if (!argv[i]) return 0;
        strings += EXEC_ALIGN8(strlen(argv[i]) + 1);
    }
    for (int i = 0; i < envc; i++) {
        if (!envp[i]) return 0;
        strings += EXEC_ALIGN8(strlen(envp[i]) + 1);
    }

    size_t qwords = 1 + (size_t)(argc + 1) + (size_t)(envc + 1);
    qwords        = (qwords + 1) & ~1ULL; // keep the final rsp 16-byte aligned

    return strings + qwords * sizeof(uint64_t) + 16;
}

int execute(const char *path, task_t *task, int argc, char *const *argv, int envc, char *const *envp) {
    if (!task || !path) {
        return -EINVAL;
    }

    char *const default_argv[1] = { (char *)path };
    if (argc <= 0 || !argv) {
        argc = 1;
        argv = default_argv;
    }
    if (envc <= 0 || !envp) {
        envc = 0;
        envp = NULL;
    }

    // Everything is placed in the top page of the user stack; check that it fits before we touch the task
    size_t args_size = exec_args_stack_size(argc, argv, envc, envp);
    if (!args_size) {
        return -EINVAL;
    }
    if (args_size > PAGE_LEN - 64) {
        return -E2BIG;
    }

    // NOTE: this is ONLY for the vfs_open() call
    char *path_abs = kmalloc_heap(PATH_MAX);
    if (!path_abs) {
        printf("OOM Error: execute(): OOM error allocating memory for absolute path buffer\n");
        return -ENOMEM;
    }

    int res = handle_relative_path(path, task, path_abs);
    if (res < 0) {
        printf("execute(): failed to handle relative path %s for task %s (tid %p), errno %d\n", path, task->name,
               task->tid, res);
        kfree_heap(path_abs);
        return res;
    }

    if (strlen(path) > EXEC_MAX_PATH) {
        return -EINVAL;
    }

    if (!linker_elf_buf) {
        load_linker();
    }

    vfs_handle_t *ld_handle = NULL;

    res = vfs_open(path_abs, SYSCALL_OPEN_FLAG_READ, 0, 0, &ld_handle);
    kfree_heap(path_abs);
    if (res != 0) {
        return res;
    }

    uint64_t file_size = ld_handle->backing_node->stat.size;
    if (file_size < sizeof(ELF_header_t) || file_size > EXEC_MAX_FILE_SIZE) {
        vfs_close(ld_handle);
        return -ENOEXEC;
    }

    void *elf_buf = kmalloc_heap(file_size);
    if (!elf_buf) {
        vfs_close(ld_handle);
        return -ENOMEM;
    }

    ssize_t read_bytes = vfs_read(ld_handle, elf_buf, file_size);
    vfs_close(ld_handle);
    if (read_bytes < 0) {
        kfree_heap(elf_buf);
        return (int)read_bytes;
    } else if ((uint64_t)read_bytes != file_size) {
        kfree_heap(elf_buf);
        return -EIO;
    }

    // New mm
    mm_unref(task->mm);
    task->mm = mm_new();
    if (!task->mm) {
        kout(KERNEL_WARN, "execute(): OOM: couldn't allocate new mm!\n");
        kfree_heap(elf_buf);
        return -ENOMEM;
    }
    task->mm_to_use = task->mm;

    // Load the linker first
    uint64_t linker_entry;
    res = loadelf(linker_elf_buf, linker_elf_size, task->mm, &linker_entry, LINKER_OFFS);
    if (res) {
        kfree_heap(elf_buf);
        return res;
    }

    uint64_t entry;
    res = loadelf(elf_buf, file_size, task->mm, &entry, 0x400000ULL);
    kfree_heap(elf_buf);
    if (res) {
        return res;
    }

    task->thread_state.ds = USER_DS | 3;
    task->thread_state.es = USER_DS | 3;
    task->thread_state.fs = USER_DS | 3;
    task->thread_state.gs = USER_DS | 3;

    // TODO: user stack (must find an open address for it as well, because multiple tasks may share a page table)
    // TODO: use an actual address for this??
    vm_area_t *user_stack = NULL;
    if ((res = vmalloc((void *)(0x0000800000000000UL - 0x100000), 0x100000, VMA_USER | VMA_RW | VMALLOC_ZERO, &user_stack, task->mm))) {
        kout(KERNEL_WARN, "execute(): couldn't alloc user stack (res %d)!\n", res);
        return res;
    }
    uint64_t ustack_top = (uint64_t)user_stack->end;

    task->thread_state.rsp = (uint64_t)task->kstack_top;

    // Everything we are about to do fits in the top page of the stack.
    uint64_t stack_top_phys = get_physaddr(ustack_top - PAGE_LEN, task->mm);
    if (!stack_top_phys) {
        return -ENOMEM;
    }
    uint64_t *rsp_og = (uint64_t *)(phys_to_virt(stack_top_phys) + 0x1000);

    // Initial user stack, SysV-style (what crt0/ld.so expect), lowest address first:
    //   argc | argv[0..argc-1] | NULL | envp[0..envc-1] | NULL | (pad) | ...strings...
    // The strings sit at the top of the page; the pointer array sits directly below them.
    size_t strings_len = 0;
    for (int i = 0; i < argc; i++) strings_len += EXEC_ALIGN8(strlen(argv[i]) + 1);
    for (int i = 0; i < envc; i++) strings_len += EXEC_ALIGN8(strlen(envp[i]) + 1);

    size_t qwords = 1 + (size_t)(argc + 1) + (size_t)(envc + 1);
    qwords        = (qwords + 1) & ~1ULL; // even, so rsp is 16-byte aligned

    uint8_t *strings_base = (uint8_t *)(((uint64_t)rsp_og - strings_len) & ~0xFULL);
    uint64_t *sp          = (uint64_t *)(strings_base - qwords * sizeof(uint64_t));

#define EXEC_USER_ADDR(kptr) (ustack_top - ((uint64_t)rsp_og - (uint64_t)(kptr)))

    memset(sp, 0, qwords * sizeof(uint64_t)); // also provides the NULL terminators and padding

    sp[0] = (uint64_t)argc;

    uint8_t *cursor = strings_base;
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        memcpy(cursor, argv[i], len);
        sp[1 + i] = EXEC_USER_ADDR(cursor);
        cursor += EXEC_ALIGN8(len);
    }
    // sp[1 + argc] is the argv NULL terminator
    for (int i = 0; i < envc; i++) {
        size_t len = strlen(envp[i]) + 1;
        memcpy(cursor, envp[i], len);
        sp[2 + argc + i] = EXEC_USER_ADDR(cursor);
        cursor += EXEC_ALIGN8(len);
    }
    // sp[2 + argc + envc] is the envp NULL terminator

    uint64_t user_rsp = EXEC_USER_ADDR(sp);
#undef EXEC_USER_ADDR

    stack_push_qword((uint64_t)USER_DS | 3, (uint64_t **)&task->thread_state.rsp);   // SS
    stack_push_qword(user_rsp, (uint64_t **)&task->thread_state.rsp);                // RSP
    stack_push_qword((uint64_t)DEFAULT_FLAGS, (uint64_t **)&task->thread_state.rsp); // RFLAGS
    stack_push_qword((uint64_t)USER_CS | 3, (uint64_t **)&task->thread_state.rsp);   // CS
    stack_push_qword((uint64_t)linker_entry, (uint64_t **)&task->thread_state.rsp);  // RIP

    stack_push_qword(user_rsp, (uint64_t **)&task->thread_state.rsp);              // for rdi
    stack_push_qword((uint64_t)LINKER_OFFS, (uint64_t **)&task->thread_state.rsp); // for rsi; linker base addr
    stack_push_qword((uint64_t)0x400000ULL, (uint64_t **)&task->thread_state.rsp); // for rdx; shell binary base addr

    stack_push_qword((uint64_t)user_task_hook, (uint64_t **)&task->thread_state.rsp);

    // Push the kernel stack now
    stack_push_qword(0, (uint64_t **)&task->thread_state.rsp); // rbp
    stack_push_qword(0, (uint64_t **)&task->thread_state.rsp); // rbx
    stack_push_qword(0, (uint64_t **)&task->thread_state.rsp); // r12
    stack_push_qword(0, (uint64_t **)&task->thread_state.rsp); // r13
    stack_push_qword(0, (uint64_t **)&task->thread_state.rsp); // r14
    stack_push_qword(0, (uint64_t **)&task->thread_state.rsp); // r15

    bool _i = irq_save();
    lock_rq();
    task_ready(task);
    unlock_rq();
    irq_restore(_i);

    return 0;
}