#include "proc/scheduler.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/cpuid/cpuid.h"
#include "arch/x86_64/irq.h"
#include "cpu/cpu.h"
#include "lib/stdio.h"
#include "memory/mm.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "timer/pit.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

volatile thread_t *volatile ready_threads      = NULL;
volatile thread_t *volatile ready_threads_last = NULL;

volatile thread_t *volatile cores_threads[MAX_CORES] = { 0 };

// static _Atomic(uint32_t) ready_threads_lock = (uint32_t)-1;
static atomic_flag ready_threads_locked_atomic = ATOMIC_FLAG_INIT;

uint32_t lock_ready_threads() {
    /* for (;;) {
        atomic
        if (ready_threads_lock == (uint32_t)-1) {
            ready_threads_lock = get_curr_core();
            return ready_threads_lock;
        }
    } */
    while (atomic_flag_test_and_set_explicit(&ready_threads_locked_atomic, __ATOMIC_ACQUIRE)) {
        // Wait
        __builtin_ia32_pause();
    }
}

void unlock_ready_threads() {
    // ready_threads_lock = (uint32_t)-1;
    atomic_flag_clear_explicit(&ready_threads_locked_atomic, __ATOMIC_RELEASE);
}

static void thread_unready_nolock(thread_t *thread) {
    if (!ready_threads) {
        return;
    }

    thread->state = ASLEEP;

    thread_t *cur = ready_threads;

    if (cur == thread) {
        ready_threads = thread->next;
        if (!ready_threads) {
            ready_threads_last = NULL;
        }
        return;
    }

    while (cur->next) {
        if (cur->next == thread) {
            cur->next = thread->next;
            break;
        }
        cur = cur->next;
    }
}

void thread_unready(thread_t *thread) {
    lock_ready_threads();

    thread_unready_nolock(thread);

    unlock_ready_threads();
}

void thread_ready(thread_t *thread) {
    asm volatile("cli");
    lock_ready_threads();

    thread->state = READY;

    if (!ready_threads) {
        ready_threads      = thread;
        ready_threads_last = thread;
    } else {
        ready_threads_last->next = thread;
        ready_threads_last       = thread;
    }

    /* for (size_t i = 0; i < get_n_cores(); i++) {
        if (cpu_cores[i].online && !cores_threads[i]) {
            // There's a core without a thread assigned, wake it up
            printf("Waking up core %d for thread %p\n", i, ready_threads->tid);
            // send_ipi(i, IPI_TLB_FLUSH_IRQ + 32); // IPI for tlb flush
            // pit_sleep(100);
            cores_threads[i] = ready_threads;
            thread_unready_nolock(ready_threads);
            send_ipi(cpu_cores[i].lapic_id, IPI_TLB_FLUSH_IRQ + 32); // IPI for tlb flush
            break;
        }
    } */

    unlock_ready_threads();
    asm volatile("sti");
}

__attribute__((noreturn)) void start_scheduler() {
    /* uint32_t curr_core = lock_ready_threads();
    printf("Starting scheduler on core %d\n", curr_core);

    for (size_t i = 0; i < get_n_cores(); i++) {
        if (!ready_threads) {
            printf("No more ready threads to assign to core %d\n", i);
            ready_threads_last = NULL;
            break;
        }

        printf("Assigning thread to core %d\n", i);

        cores_threads[i] = ready_threads;

        ready_threads = ready_threads->next;
    }

    unlock_ready_threads(); */

    cpu_cores[0].online = true;
    cpu_scheduler_task();
}

extern void restore_cpu_state(registers_t *regs);

extern void _finalize_task_switch(registers_t *regs);

void cpu_scheduler_task() {
    asm volatile("sti");

    uint32_t curr_core = get_curr_core();

    for (;;) {
        asm volatile("cli");
        // printf("core %d scheduler tick\n", get_curr_core());
        lock_ready_threads();
        if (cores_threads[curr_core]) {
            cores_threads[curr_core]->state = RUNNING;
            unlock_ready_threads();
            cpu_cores[curr_core].status |= 1;
            // There's a thread assigned to this core, start executing it
            printf("Starting thread %p on core %d\n", cores_threads[curr_core]->tid, curr_core);
            // The thread's regs are the first item in the struct
            assign_thread_to_cpu(cores_threads[curr_core]);
            cores_threads[curr_core]->state  = ASLEEP;
            cores_threads[curr_core]         = NULL;
            cpu_cores[curr_core].status     &= ~1;
        } else {
            unlock_ready_threads();
        }

        // printf("No threads to schedule on core %d, halting...\n", curr_core);
        // asm volatile("sti");
        // asm volatile("hlt");

        lock_ready_threads();

        if (ready_threads) {
            cores_threads[curr_core] = ready_threads;
            thread_unready_nolock(ready_threads);
            printf("Assigned thread %p to core %d\n", cores_threads[curr_core]->tid, curr_core);
            unlock_ready_threads();
            asm volatile("sti");
            // pit_sleep(100);
            continue;
        }

        unlock_ready_threads();

        // printf("No threads to schedule on core %d, halting...\n", curr_core);
        asm volatile("sti");
        // asm volatile("hlt");
        pit_sleep(100);
    }
}

void assign_thread_to_cpu(thread_t *thread) {
    asm volatile("cli");
    // Check if kernel page table needs to be flushed
    if (get_physaddr((uint64_t)&thread->regs, kernel_pml4) == 0x0) {
        ipi_tlb_flush_routine(NULL);
    }

    thread->base->processor_id     = get_curr_core();
    thread->base->stack            = (uint64_t)thread->regs.iret_rsp;
    thread->base->cr3              = virt_to_phys((uint64_t)thread->parent->pml4) & ~0xFFF;
    thread->base->proc             = (uint64_t)thread->parent;
    thread->base->kernel_pml4_phys = (uint64_t)kernel_pml4_phys;

    // The thread's regs are the first item in the struct
    printf("Thread info: regs addr %p, regs phys addr %p %p, stack %p, base %p, base proc %p\n", &thread->regs,
           get_physaddr((uint64_t)&thread->regs, thread->parent->pml4),
           get_physaddr((uint64_t)&thread->regs, kernel_pml4), thread->regs.iret_rsp, thread->base, thread->base->proc);

    // write_msr(IA32_KERNEL_GS_BASE, (uint64_t)thread->base);
    write_msr(IA32_GS_BASE, (uint64_t)thread->base);

    _finalize_task_switch((registers_t *)&thread->regs);
}