#include "proc/thread.h"

#include "arch/x86_64/gdt/gdt.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "proc/proc.h"

#include <stdatomic.h>
#include <stdint.h>

pid_t next_tid = 0;

#define DEFAULT_FLAGS 0x202 // Enable interrupts

// Stacks start below this address and grow downwards
#define USER_STACK_TOP 0x0000800000000000UL

thread_t *create_thread(proc_t *proc, const char *name, void (*func)(void *), void *arg) {
    if (!proc) return NULL;

    thread_t *thread = (thread_t *)phys_to_virt(find_next_free_frame());
    // map_virtual_memory(virt_to_phys((uint64_t)thread), PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, kernel_pml4);
    memset(thread, 0, PAGE_LEN);

    if (name) {
        strncpy(thread->name, name, PROCESS_THREAD_NAME_LEN);
        thread->name[PROCESS_THREAD_NAME_LEN - 1] = '\0';
    } else {
        thread->name[0] = 0;
    }

    thread->next            = NULL;
    thread->parent          = proc;
    // thread->regs = {0};
    thread->state           = ASLEEP;
    thread->tid             = next_tid++;
    thread->index_in_parent = atomic_fetch_add(&(proc->next_thread_index), 1);

    uint64_t p = 0;

    thread->stack_top = (void *)USER_STACK_TOP - (thread->index_in_parent * (THREAD_MAX_STACK_SIZE + 0x1000));
    // Skip guard page
    for (uint64_t i = 0; i < THREAD_STACK_SIZE; i += PAGE_LEN) {
        p = alloc_virtual_memory((uint64_t)thread->stack_top - i - PAGE_LEN, ALLOCATE_VM_USER, proc->pml4);
        if (!p) {
            printf("Couldn't allocate stack for user process. Halt!\n");
            hcf();
        }
        memset((void *)phys_to_virt(p), 0, PAGE_LEN);
    }

    if (!thread->stack_top) {
        next_tid--;
        return NULL;
    }

    // Set ss table descriptors
    thread->regs.iret_cs = USER_CS; // User code
    thread->regs.iret_ss = USER_DS; // User stack/data

    thread->regs.ds = USER_DS;
    thread->regs.es = USER_DS;
    thread->regs.fs = USER_DS;
    thread->regs.gs = USER_DS;

    // Set stack align
    thread->regs.iret_rsp = (uint64_t)thread->stack_top;

    // Enable interrupts
    thread->regs.iret_rflags = DEFAULT_FLAGS;

    // Set the instruction pointer and the function argument
    // map_virtual_memory_using_alloc(get_physaddr((uint64_t)func), (uint64_t)func, 80000, PAGE_FLAG_PRESENT |
    // PAGE_FLAG_USER, alloc_paging_node, proc->pml4); map_virtual_memory_using_alloc(get_physaddr((uint64_t)arg),
    // (uint64_t)arg, 80000, PAGE_FLAG_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITE, alloc_paging_node, proc->pml4);
    thread->regs.iret_rip = (uint64_t)func;
    printf("Thread info: func addr %p, func phys addr %p\n", func, get_physaddr((uint64_t)func, proc->pml4));
    thread->regs.rdi = (uint64_t)arg;

    // Set the page table
    thread->regs.cr3 = virt_to_phys((uint64_t)proc->pml4) & ~0xFFF;

    thread->base = (gsbase_t *)phys_to_virt(find_next_free_frame());
    memset(thread->base, 0, PAGE_LEN);

    // These values will be set when the thread is assigned to a CPU
    /* thread->base->proc             = (uint64_t)proc;
    thread->base->cr3              = thread->regs.cr3;
    thread->base->stack            = (uint64_t)thread->regs.iret_rsp;
    thread->base->kernel_pml4_phys = (uint64_t)kernel_pml4_phys; */

    map_virtual_memory_using_alloc(virt_to_phys((uint64_t)thread), (uint64_t)thread, PAGE_LEN,
                                   PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, alloc_paging_node, proc->pml4);
    map_virtual_memory_using_alloc(virt_to_phys((uint64_t)thread->base), (uint64_t)thread->base, PAGE_LEN,
                                   PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, alloc_paging_node, proc->pml4);

    if (proc->threads == NULL) {
        proc->threads = thread;
    } else {
        thread_t *t = proc->threads;

        while (t->next) {
            t = t->next;
        }

        t->next = thread;
    }

    return thread;
}

void set_thread_state(thread_t *thread, proc_thread_state_t state) {
    thread->state = state;
}

void release_thread(thread_t *thread) {
    if (!thread) return;
}