#include "proc/proc.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/cpuid/cpuid.h"
#include "arch/x86_64/gdt/tss.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"
#include "cpu/cpu.h"
#include "exec/elf.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "proc/scheduler.h"
#include "proc/thread.h"
#include "vfs/fscache.h"

#include <stdint.h>

volatile proc_t *pid_zero = NULL;

volatile pid_t next_pid = 0;

static fixed_size_allocator_t _proc_fsa = FSA_DEFAULT(sizeof(proc_t));

proc_t *_get_proc_kernel() {
    gsbase_t *gsbase = (gsbase_t *)read_msr(IA32_GS_BASE);

    if (!gsbase) {
        printf("Error: gsbase is NULL in _get_proc_kernel!\n");
        return NULL;
    }
    return (proc_t *)gsbase->proc;
}

// TODO: enforce some (configurable) PID_MAX?
proc_t *create_proc(const char *name, proc_t *parent) {
    if (!parent) {
        if (next_pid != 0) {
            printf("Error: trying to create a new process (name %s), but pid 0 is taken and no parent was provided!\n",
                   name);
            return NULL;
        }
    }

    proc_t *proc = (proc_t *)fsa_alloc(&_proc_fsa);
    if (!proc) {
        kout(KERNEL_WARN, "create_proc(): fsa_alloc() returned NULL (OOM?)\n");
        return NULL;
    }

    uint64_t cwd_frame = find_next_free_frame();
    if (!cwd_frame) {
        kout(KERNEL_WARN, "Error allocating memory for proc cwd (OOM?).\n");
        fsa_free(&_proc_fsa, proc);
        return NULL;
    }
    char *cwd = (char *)phys_to_virt(cwd_frame);
    memset(cwd, 0, PAGE_LEN);
    proc->cwd = cwd;

    if (name) {
        strncpy(proc->name, name, PROCESS_THREAD_NAME_LEN);
        proc->name[PROCESS_THREAD_NAME_LEN - 1] = '\0';
    } else {
        proc->name[0] = 0;
    }

    proc->first_child  = NULL;
    proc->next_sibling = NULL;
    proc->parent       = parent;

    if (parent) {
        proc->uid = parent->uid;
        strncpy(proc->cwd, parent->cwd, PATH_MAX);
        proc->cwd[PATH_MAX - 1] = '\0';
    } else {
        proc->uid = 0;
        strncpy(proc->cwd, "/", PATH_MAX);
        proc->cwd[PATH_MAX - 1] = '\0';
    }

    proc->pid = next_pid++;
    if (proc->pid == 0) {
        pid_zero = proc;
    }
    proc->state   = READY;
    proc->threads = NULL;

    // TODO: page table
    proc->pml4 = (pml4_t *)phys_to_virt(find_next_free_frame());
    if (!proc->pml4) {
        kout(KERNEL_WARN, "Error allocating page table for proc %s (%lu) (OOM?)\n", proc->name, proc->pid);
        phys_mem_unref_frame(phys_addr_to_frame_struct(cwd_frame));
        fsa_free(&_proc_fsa, proc);
        return NULL;
    }
    memset(proc->pml4, 0, PAGE_LEN);

    printf("proc pml4 virt: %p\n", proc->pml4);

    // TODO: handle OOM from map_virtual_memory_using_alloc
    for (uint64_t i = TSS_STACK_ADDR - (KERNEL_STACK_SIZE * get_n_cores()); i < TSS_STACK_ADDR; i += PAGE_LEN) {
        // printf("mapping %p\n", gs_bases[i]);
        map_virtual_memory_using_alloc(get_physaddr(i, kernel_pml4), i, PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE,
                                       alloc_paging_node, proc->pml4);
    }

    // TODO: only map the necessary regions (interrupt tables + stubs only?)
    map_virtual_memory_using_alloc(kernel_load_phys, kernel_load_virt, 1 << 20 /* 2 MiB */,
                                   PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, alloc_paging_node, proc->pml4);
    map_virtual_memory_using_alloc(virt_to_phys(IOAPIC_ADDR), IOAPIC_ADDR, 4096, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE,
                                   alloc_paging_node, proc->pml4);
    map_virtual_memory_using_alloc(virt_to_phys(LAPIC_BASE), LAPIC_BASE, 4096, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE,
                                   alloc_paging_node, proc->pml4);

    for (uint32_t i = 0; (i < MAX_CORES) && (gs_bases[i]); i++) {
        // printf("mapping %p\n", gs_bases[i]);
        map_virtual_memory_using_alloc(get_physaddr((uint64_t)gs_bases[i], kernel_pml4), (uint64_t)gs_bases[i],
                                       PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, alloc_paging_node, proc->pml4);
    }

    proc->fds_max = PROCESS_FDS_MAX;

    size_t fds_size = sizeof(vfs_handle_t *) * proc->fds_max;

    proc->fds = kmalloc_heap(fds_size);
    memset((void *)proc->fds, 0, fds_size);

    if (parent) {
        // Insert this proc into the list
        if (parent->first_child) {
            proc->next_sibling = parent->first_child;
        }

        parent->first_child = proc;
    }

    return proc;
}

void *linker_elf_buf = NULL;

void load_linker() {
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

void stack_push_qword(uint64_t qword, uint64_t **rsp) {
    *rsp  = (uint64_t *)((uint8_t *)(*rsp) - sizeof(uint64_t));
    **rsp = qword;
}

int execute(const char *path, proc_t *proc) {
    if (!linker_elf_buf) {
        load_linker();
    }

    vfs_handle_t *ld_handle = NULL;

    int res = vfs_open(path, SYSCALL_OPEN_FLAG_READ, 0, 0, &ld_handle);
    if (res == 0) {
        uint64_t file_size = ld_handle->backing_node->stat.size;

        uint64_t entry;
        void *elf_buf = kmalloc_heap(file_size);
        if (!elf_buf) {
            return -ENOMEM;
        }

        ssize_t read_bytes = vfs_read(ld_handle, elf_buf, file_size);
        vfs_close(ld_handle);
        if (read_bytes < 0) {
            return (int)read_bytes;
        } else if ((uint64_t)read_bytes != file_size) {
            return -EIO;
        }

        // Load the linker first
        uint64_t linker_entry;
        loadelf(linker_elf_buf, proc->pml4, &linker_entry, LINKER_OFFS);
        // kfree_heap(linker_elf_buf);

        // 0x7fff80000000ULL
        loadelf(elf_buf, proc->pml4, &entry, 0x400000ULL);
        kfree_heap(elf_buf);

        thread_t *thread = create_thread(proc, path, (void *)linker_entry, NULL);

        // Everything we are about to do fits in the top page of the stack.
        uint64_t *rsp_og
            = (uint64_t *)(phys_to_virt(get_physaddr((uint64_t)thread->stack_top - PAGE_LEN, proc->pml4)) + 0x1000);
        uint64_t *rsp = rsp_og;

        // First, setup the stack
        const char *argv0 = path;
        const char *argv1 = NULL;

        const char *envp0 = NULL;

        const uint64_t argc = 1;

        size_t argv0_len = strlen(argv0) + 1;

        // Push argv0 onto the stack (getting address aligned to 8 bytes)
        uint64_t argv0_addr = ((uint64_t)(rsp)-argv0_len) & ~0x7ULL;
        rsp                 = (uint64_t *)argv0_addr;
        memcpy((void *)argv0_addr, argv0, argv0_len);

        uint64_t str_usr_addr = (uint64_t)thread->stack_top - ((uint64_t)rsp_og - argv0_addr);

        // Align the rsp to 16-bytes (we have even number of values left to push after this)
        rsp = (uint64_t *)((uint64_t)(rsp) & ~0xF);

        // Push envp0, argv1, argv0, argc onto the stack
        stack_push_qword(0, &rsp);
        stack_push_qword(0, &rsp);
        stack_push_qword((uint64_t)str_usr_addr, &rsp);
        stack_push_qword((uint64_t)argc, &rsp);

        thread->regs.iret_rsp -= (uint64_t)rsp_og - (uint64_t)rsp;

        thread->regs.rdi = thread->regs.iret_rsp;
        thread->regs.rsi = LINKER_OFFS; // Linker base addr
        thread->regs.rdx = 0x400000ULL; // Shell binary base addr

        asm volatile("cli");
        lock_thread_rq();

        thread_ready(thread);

        unlock_thread_rq();
        asm volatile("sti");

        return 0;
    }

    return res;
}

void process_exit(proc_t *proc) {
    if (!proc) return;

    if (proc->pid == 0) {
        kout(KERNEL_PANIC, "\n\n[ FATAL ERROR: process_exit called on pid 0! ]\n");
    }

    // Kill all child processes
    while (proc->first_child) {
        process_exit(proc->first_child);
    }

    kill_process_threads(proc);

    // Free all threads
    thread_t *thread = (thread_t *)proc->threads;
    thread_t *next_thread;

    while (thread) {
        next_thread = thread->next;

        // Free stack
        if (thread->stack_top) {
            for (uint64_t i = 0; i < THREAD_MAX_STACK_SIZE; i += PAGE_LEN) {
                uint64_t phys = get_physaddr((uint64_t)thread->stack_top - i, proc->pml4);
                if (phys) {
                    phys_mem_unref_frame(phys_addr_to_frame_struct(phys));
                }
            }
        }
        phys_mem_unref_frame(phys_addr_to_frame_struct(virt_to_phys((uint64_t)thread->base)));
        phys_mem_unref_frame(phys_addr_to_frame_struct(virt_to_phys((uint64_t)thread)));

        thread = next_thread;
    }

    // Unref page table
    free_page_table((pml4_t *)proc->pml4);

    proc_t **link = proc->parent ? &proc->parent->first_child : NULL;

    while (*link && *link != proc) {
        link = &((*link)->next_sibling);
    }

    if (!*link) {
        kout(KERNEL_PANIC, "ERROR: when destroying process %lu (%s), we couldn't find it in it's parent's processes!\n",
             proc->pid, proc->name);
    } else {
        *link = proc->next_sibling;
    }

    fsa_free(&_proc_fsa, proc);
}

vfs_handle_t *proc_get_fd(proc_t *proc, int fd) {
    if (!proc) {
        return NULL;
    }

    if (fd < 0) {
        printf("proc_get_fd: fd %d out of range (negative) for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return NULL;
    }

    if (fd >= proc->fds_max) {
        printf("proc_get_fd: fd %d out of range (too high) for process %s (pid %p)\n", fd, proc->name, proc->pid);
        return NULL;
    }

    return proc->fds[fd];
}

int proc_alloc_fd(proc_t *proc, vfs_handle_t *handle) {
    if (!handle) {
        printf("proc_alloc_fd: handle is NULL\n");
        return -1;
    }

    for (int i = 0; i < proc->fds_max; i++) {
        if (proc->fds[i] == NULL) {
            proc->fds[i] = handle;
            return i;
        }
    }

    return -1;
}

// WARNING: this does *not* free the underlying vfs_handle_t!
void proc_free_fd(proc_t *proc, int fd) {
    if (fd >= proc->fds_max) {
        return;
    }

    proc->fds[fd] = NULL;
}