#include "proc/thread.h"

#include "arch/x86_64/gdt/gdt.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "memory/kmalloc.h"
#include "memory/mm.h"
#include "proc/proc.h"

#include <stdatomic.h>
#include <stdint.h>

pid_t next_tid = 0;

static fixed_size_allocator_t _thread_fsa = FSA_DEFAULT(sizeof(thread_t));

#define DEFAULT_FLAGS 0x202 // Enable interrupts

// Stacks start below this address and grow downwards
#define USER_STACK_TOP 0x0000800000000000UL

thread_t *create_thread(proc_t *proc, const char *name, void (*func)(void *), void *arg) {
    if (!proc) return NULL;

    thread_t *thread = (thread_t *)fsa_alloc(&_thread_fsa);
    if (!thread) {
        kout(KERNEL_WARN, "create_thread(): fsa_alloc() returned NULL (OOM?)\n");
        return NULL;
    }

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
    thread->tid             = __atomic_fetch_add(&next_tid, 1, __ATOMIC_SEQ_CST);
    thread->index_in_parent = atomic_fetch_add(&(proc->next_thread_index), 1);

    uint64_t p = 0;

    thread->stack_top = (void *)USER_STACK_TOP - (thread->index_in_parent * (THREAD_MAX_STACK_SIZE + 0x1000));
    // Skip guard page
    for (uint64_t i = 0; i < THREAD_STACK_SIZE; i += PAGE_LEN) {
        p = alloc_virtual_memory((uint64_t)thread->stack_top - i - PAGE_LEN, ALLOCATE_VM_USER, proc->pml4);
        if (p < PAGE_LEN) {
            kout(KERNEL_WARN, "create_thread(): couldn't allocate stack for thread (OOM?)\n");
            for (uint64_t j = 0; j < i; j += PAGE_LEN) {
                free_virtual_memory((uint64_t)thread->stack_top - j - PAGE_LEN, proc->pml4);
            }
            fsa_free(&_thread_fsa, thread);
            return NULL;

        }
        memset((void *)phys_to_virt(p), 0, PAGE_LEN);
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
    thread->regs.iret_rip = (uint64_t)func;
    printf("Thread info: func addr %p, func phys addr %p\n", func, get_physaddr((uint64_t)func, proc->pml4));
    thread->regs.rdi = (uint64_t)arg;

    // Set the page table
    thread->regs.cr3 = virt_to_phys((uint64_t)proc->pml4) & ~0xFFF;

    uint64_t base_frame = find_next_free_frame();
    if (!base_frame) {
        kout(KERNEL_WARN, "create_thread(): couldn't allocate gsbase page (OOM?)\n");
        for (uint64_t j = 0; j < THREAD_STACK_SIZE; j += PAGE_LEN) {
            free_virtual_memory((uint64_t)thread->stack_top - j - PAGE_LEN, proc->pml4);
        }
        fsa_free(&_thread_fsa, thread);
        return NULL;
    }
    thread->base = (gsbase_t *)phys_to_virt(base_frame);
    memset(thread->base, 0, PAGE_LEN);

    // These values will be set when the thread is assigned to a CPU
    /* thread->base->proc             = (uint64_t)proc;
    thread->base->cr3              = thread->regs.cr3;
    thread->base->stack            = (uint64_t)thread->regs.iret_rsp;
    thread->base->kernel_pml4_phys = (uint64_t)kernel_pml4_phys; */

    // TODO: handle OOM in these
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

    // Free stack
    if (thread->stack_top) {
        for (uint64_t i = 0; i < THREAD_MAX_STACK_SIZE; i += PAGE_LEN) {
            uint64_t phys = get_physaddr((uint64_t)thread->stack_top - i, thread->parent->pml4);
            if (phys) {
                phys_mem_unref_frame(phys_addr_to_frame_struct(phys));
            }
        }
    }
    phys_mem_unref_frame(phys_addr_to_frame_struct(virt_to_phys((uint64_t)thread->base)));

    fsa_free(&_thread_fsa, thread);
}