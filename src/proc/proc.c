#include "proc/proc.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/cpuid/cpuid.h"
#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/gdt/tss.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"
#include "cpu/cpu.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "proc/scheduler.h"
#include "proc/thread.h"

#include <stdint.h>

volatile proc_t *pid_zero = NULL;

volatile uint64_t next_pid = 0;

proc_t *_get_proc_kernel() {
    gsbase_t *gsbase = (gsbase_t *)read_msr(IA32_GS_BASE);

    if (!gsbase) {
        printf("Error: gsbase is NULL in _get_proc_kernel!\n");
        return NULL;
    }
    return (proc_t *)gsbase->proc;
}

proc_t *create_proc(const char *name, proc_t *parent) {
    if (!parent) {
        if (next_pid != 0) {
            printf("Error: trying to create a new process (name %s), but pid 0 is taken and no parent was provided!\n",
                   name);
            return NULL;
        }
    }

    proc_t *proc = (proc_t *)phys_to_virt(find_next_free_frame());

    if (!proc) {
        printf("Error allocating memory for proc_t.\n");
        return NULL;
    }

    char *cwd = (char *)phys_to_virt(find_next_free_frame());
    if (!cwd) {
        printf("Error allocating memory for proc cwd.\n");
        phys_mem_unref_frame((phys_mem_free_frame_t *)phys_addr_to_frame_addr(virt_to_phys((uint64_t)proc)));
        return NULL;
    }

    memset(proc, 0, PAGE_LEN);
    memset(cwd, 0, PAGE_LEN);
    proc->cwd = cwd;

    if (name) {
        strncpy(proc->name, name, PROCESS_THREAD_NAME_LEN);
        proc->name[PROCESS_THREAD_NAME_LEN - 1] = '\0';
    } else {
        proc->name[0] = 0;
    }

    proc->first_child  = NULL;
    proc->prev_sibling = NULL;
    proc->next_sibling = NULL;
    proc->parent       = parent;

    if (parent) {
        proc->uid = parent->uid;
        strncpy(proc->cwd, parent->cwd, PATH_MAX);
    } else {
        proc->uid = 0;
        strncpy(proc->cwd, "/", PATH_MAX);
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
        printf("Error allocating page table for proc %s (%lu)\n", proc->name, proc->pid);
        return NULL;
    }
    memset(proc->pml4, 0, PAGE_LEN);

    printf("proc pml4 virt: %p\n", proc->pml4);

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
            proc->next_sibling                = parent->first_child;
            parent->first_child->prev_sibling = proc;
        }

        parent->first_child = proc;
    }

    return proc;
}

void process_exit(proc_t *proc) {
    if (!proc) return;

    if (proc->pid == 0) {
        printf("\n\n[ FATAL ERROR: process_exit called on pid 0! ]\n");
        hcf();
    }

    // Kill all child processes
    while (proc->first_child) {
        process_exit(proc->first_child);
    }

    lock_ready_threads();

    for (size_t i = 0; i < MAX_CORES; i++) {
        if (cores_threads[i] && (cores_threads[i]->parent == proc)) {
            if (cores_threads[i]->state == RUNNING) {
                // We're currently running on this core, we need to switch to another thread
                printf("Process exit called on core %d, switching to another thread...\n", i);
                send_ipi(cpu_cores[i].lapic_id, IPI_KILL_IRQ + 32); // IPI for kill
            }

            // This core is running a thread from this process, stop it
            cores_threads[i]->state  = ASLEEP;
            cores_threads[i]         = NULL;
            cpu_cores[i].status     &= ~1;
        }
    }

    // Free all threads
    thread_t *thread = (thread_t *)proc->threads;
    thread_t *next_thread;

    while (thread) {
        next_thread = thread->next;

        // Free stack
        if (thread->stack) {
            for (uint64_t i = 0; i < THREAD_STACK_SIZE; i += PAGE_LEN) {
                phys_mem_unref_frame((phys_mem_free_frame_t *)phys_addr_to_frame_addr(
                    get_physaddr((uint64_t)thread->stack + i, proc->pml4)));
            }
        }
        phys_mem_unref_frame((phys_mem_free_frame_t *)phys_addr_to_frame_addr(virt_to_phys((uint64_t)thread->base)));
        phys_mem_unref_frame((phys_mem_free_frame_t *)phys_addr_to_frame_addr(virt_to_phys((uint64_t)thread)));

        thread = next_thread;
    }

    unlock_ready_threads();

    // Unref page table
    free_page_table((pml4_t *)proc->pml4);

    proc_t *tmp = proc->parent ? proc->parent->first_child : NULL;

    while (tmp) {
        if (tmp == proc) {
            if (tmp->prev_sibling) {
                tmp->prev_sibling->next_sibling = tmp->next_sibling;
            }

            if (tmp->next_sibling) {
                tmp->next_sibling->prev_sibling = tmp->prev_sibling;
            }

            break;
        }
        tmp = tmp->next_sibling;
    }

    if (!tmp) {
        printf("ERROR: when destroying process %lu (%s), we couldn't find it in it's parent's processes!\n", proc->pid,
               proc->name);
    }

    phys_mem_unref_frame((phys_mem_free_frame_t *)phys_addr_to_frame_addr(virt_to_phys((uint64_t)proc)));
}

vfs_handle_t *proc_get_fd(proc_t *proc, size_t fd) {
    if (fd >= proc->fds_max) {
        return NULL;
    }

    return proc->fds[fd];
}

ssize_t proc_alloc_fd(proc_t *proc, vfs_handle_t *handle) {
    for (size_t i = 0; i < proc->fds_max; i++) {
        if (proc->fds[i] == NULL) {
            proc->fds[i] = handle;
            return (ssize_t)i;
        }
    }

    return -1;
}

// WARNING: this does *not* free the underlying vfs_handle_t!
void proc_free_fd(proc_t *proc, size_t fd) {
    if (fd >= proc->fds_max) {
        return;
    }

    proc->fds[fd] = NULL;
}