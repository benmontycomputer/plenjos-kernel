#include <stdint.h>

#include "proc/proc.h"
#include "proc/thread.h"

#include "memory/mm.h"
#include "memory/kmalloc.h"

#include "arch/x86_64/gdt/gdt.h"

#include "lib/string.h"
#include "lib/stdio.h"

uint64_t next_tid = 0;

#define DEFAULT_FLAGS 0x202 // Enable interrupts

thread_t *create_thread(proc_t *proc, const char *name, void (*func)(void *), void *arg) {
    if (!proc) return NULL;

    thread_t *thread = kmalloc_heap(sizeof(thread_t));

    if (name) strncpy(thread->name, name, PROCESS_THREAD_NAME_LEN);
    else
        thread->name[0] = 0;

    thread->next = NULL;
    thread->parent = proc;
    // thread->regs = {0};
    thread->state = ASLEEP;
    thread->tid = next_tid++;

    thread->stack = kmalloc_heap(THREAD_STACK_SIZE);

    if (!thread->stack) {
        // Abort
        kfree_heap(thread->stack);
        next_tid--;
        return NULL;
    }

    memset(&(thread->regs), 0, sizeof(registers_t));

    // Set ss table descriptors
    thread->regs.iret_cs = USER_CS; // User code
    thread->regs.iret_ss = USER_DS; // User stack/data

    // Set stack align
    thread->regs.iret_rsp = ((uint64_t)thread->stack + THREAD_STACK_SIZE);

    // Enable interrupts
    thread->regs.iret_rflags = DEFAULT_FLAGS;

    // Set the instruction pointer and the function argument
    // map_virtual_memory_using_alloc(get_physaddr((uint64_t)func), (uint64_t)func, 80000, PAGE_FLAG_PRESENT | PAGE_FLAG_USER, alloc_paging_node, proc->pml4);
    // map_virtual_memory_using_alloc(get_physaddr((uint64_t)arg), (uint64_t)arg, 80000, PAGE_FLAG_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITE, alloc_paging_node, proc->pml4);
    thread->regs.iret_rip = (uint64_t)func;
    printf("Thread info: func addr %p, func phys addr %p\n", func, get_physaddr((uint64_t)func));
    thread->regs.rdi = (uint64_t)arg;

    // Set the page table
    thread->regs.cr3 = virt_to_phys((uint64_t)proc->pml4) & ~0xFFF;

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