#include "proc/scheduler.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/common.h"
#include "arch/x86_64/cpuid/cpuid.h"
#include "arch/x86_64/irq.h"
#include "cpu/cpu.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "memory/mm.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "timer/pit.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_KERNEL_TASKS 8192

/* TODO: IMPORTANT: if we have 2 kernel tasks alternating time on one CPU, their stacks would currently be the same,
 * leading to corruption as RSP is switched but push/pop could interfere with the other process' stack. Each kernel task
 * should have its own stack if it can alternate time with another. */

/**
 * Note: most of these function should not be called with interrupts enabled.
 */

typedef struct kernel_task kernel_task_t;

struct kernel_task {
    _Atomic(kernel_task_func_t) func;
    void *arg;

    kernel_task_t *rq_next;
};

fixed_size_allocator_t _ktask_fsa = FSA_DEFAULT(sizeof(kernel_task_t));

thread_t *cores_threads[MAX_CORES] = { 0 };

static thread_t ktask_thread_placeholder = { 0 };

thread_t *thread_rq     = NULL;
kernel_task_t *ktask_rq = NULL;

static atomic_flag cores_threads_lock = ATOMIC_FLAG_INIT;
static atomic_flag thread_rq_lock     = ATOMIC_FLAG_INIT;
static atomic_flag ktask_rq_lock      = ATOMIC_FLAG_INIT;

// Set by kill_process_threads() for a remote core that is still running
static _Atomic bool kill_pending[MAX_CORES];

// How long kill_process_threads() waits for a remote core to acknowledge before giving up and warning
#define KILL_ACK_SPIN_LIMIT 200000000ULL

void lock_cores_threads() {
    while (atomic_flag_test_and_set_explicit(&cores_threads_lock, __ATOMIC_ACQUIRE)) {
        // Wait
        __builtin_ia32_pause();
    }
}

void unlock_cores_threads() {
    atomic_flag_clear_explicit(&cores_threads_lock, __ATOMIC_RELEASE);
}

void lock_thread_rq() {
    while (atomic_flag_test_and_set_explicit(&thread_rq_lock, __ATOMIC_ACQUIRE)) {
        // Wait
        __builtin_ia32_pause();
    }
}

void unlock_thread_rq() {
    atomic_flag_clear_explicit(&thread_rq_lock, __ATOMIC_RELEASE);
}

void lock_ktask_rq() {
    while (atomic_flag_test_and_set_explicit(&ktask_rq_lock, __ATOMIC_ACQUIRE)) {
        // Wait
        __builtin_ia32_pause();
    }
}

void unlock_ktask_rq() {
    atomic_flag_clear_explicit(&ktask_rq_lock, __ATOMIC_RELEASE);
}

thread_t *thread_rq_pop() {
    if (!thread_rq) {
        return NULL;
    }

    thread_t *res = thread_rq;

    thread_rq    = thread_rq->rq_next;
    res->rq_next = NULL;

    return res;
}

thread_t *thread_rq_remove(thread_t *thread) {
    if (!thread_rq) {
        return NULL;
    }

    thread_t **link = &thread_rq;

    while (*link && *link != thread) {
        link = &((*link)->rq_next);
    }

    if (*link) {
        *link = (*link)->rq_next;

        thread->rq_next = NULL;

        return thread;
    } else {
        // Shouldn't happen
        return NULL;
    }
}

void thread_rq_add(thread_t *thread) {
    thread->rq_next = NULL;

    thread_t **link = &thread_rq;

    while (*link) {
        link = &((*link)->rq_next);
    }

    *link = thread;
}

// NOTE: caller must still call cpu_scheduler_task(); this function does not IPI the current core as the IPI would be received later on and interrupt something else.
void kill_process_threads(proc_t *proc) {
    uint32_t cores_to_kill[MAX_CORES];
    size_t cores_to_kill_count = 0;

    uint32_t curr_core = get_curr_core();

    lock_cores_threads();
    lock_thread_rq();

    thread_t *thread = proc->threads;
    while (thread) {
        thread_rq_remove(thread);
        thread->state = DEAD;
        thread        = thread->next;
    }

    // TODO: how to eliminate any other references to this thread (scheduled I/O, wakeups, etc.)?

    unlock_thread_rq();

    for (size_t i = 0; i < MAX_CORES; i++) {
        if (cores_threads[i] && (cores_threads[i]->parent == proc)) {
            cores_threads[i]->state = DEAD;
            cores_threads[i]        = NULL;

            if (i != curr_core) {
                atomic_store_explicit(&kill_pending[i], true, __ATOMIC_RELEASE);
                cores_to_kill[cores_to_kill_count++] = i;
            }
        }
    }

    unlock_cores_threads();

    for (size_t i = 0; i < cores_to_kill_count; i++) {
        // The target core must be able to acquire cores_threads_lock in its IPI handler.
        printf("Process exit called on core %d, switching core %d to another thread...\n", curr_core,
               cores_to_kill[i]);
        send_ipi(cpu_cores[cores_to_kill[i]].lapic_id, IPI_KILL_IRQ + 32);
    }

    // Wait for the targeted cores to leave the dying threads
    for (size_t i = 0; i < cores_to_kill_count; i++) {
        uint64_t spins = 0;

        while (atomic_load_explicit(&kill_pending[cores_to_kill[i]], __ATOMIC_ACQUIRE)) {
            if (++spins > KILL_ACK_SPIN_LIMIT) {
                printf("WARN: kill_process_threads(): core %d did not acknowledge the kill IPI in time; continuing "
                       "anyway.\n",
                       cores_to_kill[i]);
                break;
            }
            __builtin_ia32_pause();
        }
    }
}

// NOTE: this just forces a schedule call; it does NOT update thread state
// It only does anything if kill_process_threads() flagged this core; otherwise it keeps running
void ipi_kill_routine(registers_t *regs, void *data) {
    uint32_t curr_core = get_curr_core();

    if (!cpu_cores[curr_core].online) {
        apic_send_eoi();
        asm volatile("cli");
        hcf();
    }

    if (!atomic_load_explicit(&kill_pending[curr_core], __ATOMIC_ACQUIRE)) {
        // irq_handler() sends the EOI when we return
        return;
    }

    kout(KERNEL_INFO, "Received kill IPI on core %d, switching to another thread...\n", curr_core);

    // At this point, we won't ever return, so this must be sent.
    apic_send_eoi();

    // cpu_scheduler_task_c() resets this core's GS base and acknowledges the kill.
    cpu_scheduler_task();
}

// TODO: differentiate between wakeup and preempt? or change wakeup to preempt?
void ipi_wakeup_routine(registers_t *regs, void *data) {
    // kout(KERNEL_INFO, "Received IPI wakeup on core %d\n", get_curr_core());

    lock_cores_threads();

    uint32_t curr_core = get_curr_core();

    if (cores_threads[curr_core] == &ktask_thread_placeholder) {
        unlock_cores_threads();
        return;
    }

    if (cores_threads[curr_core]) {
        if (cores_threads[curr_core]->state == RUNNING) {
            cores_threads[curr_core]->state = READY;

            memcpy(&cores_threads[curr_core]->regs, regs, sizeof(*regs));

            lock_thread_rq();
            thread_rq_add(cores_threads[curr_core]);
            unlock_thread_rq();
        }
        cores_threads[curr_core] = NULL;
    }

    unlock_cores_threads();

    // Won't return; have to send this
    apic_send_eoi();

    cpu_scheduler_task();
}

// TODO: more efficient way to wakeup than just sending to all cores
void send_wakeup_ipi() {
    uint32_t curr_core = get_curr_core();

    for (size_t i = 0; i < get_n_cores(); i++) {
        if (cpu_cores[i].online && i != curr_core) {
            // There's a core without a thread assigned, wake it up
            // printf("Waking up core %d\n", i);
            send_ipi(cpu_cores[i].lapic_id, IPI_WAKEUP_IRQ + 32); // IPI for wakeup
        }
    }
}

void thread_ready(thread_t *thread) {
    thread->state = READY;
    thread_rq_add(thread);

    // TODO: should this be part of scheduler tick or something?
    send_wakeup_ipi();
}

__attribute__((noreturn)) //
void start_scheduler() {
    cpu_cores[0].online = true;
    cpu_scheduler_task();
}

void stop_scheduler() {
    for (uint32_t i = 0; i < get_n_cores(); i++) {
        cpu_cores[i].online = false;
        send_ipi(cpu_cores[i].lapic_id, IPI_KILL_IRQ + 32);
    }
}

__attribute__((noreturn)) //
extern void _finalize_task_switch(registers_t *regs);

__attribute__((noreturn)) //
void cpu_scheduler_task_c(uint32_t curr_core) {
    // Clear the core's current process and tell kill_process_threads() that this core has left the old thread.
    cpu_cores[curr_core].kernel_gs_base->proc = 0;
    atomic_store_explicit(&kill_pending[curr_core], false, __ATOMIC_RELEASE);

    if (!cpu_cores[curr_core].online) {
        hcf();
    }

    lock_ktask_rq();
    if (ktask_rq) {
        // There's a kernel task to run
        kernel_task_func_t func = ktask_rq->func;
        void *arg               = ktask_rq->arg;


        kernel_task_t *old = ktask_rq;
        ktask_rq = ktask_rq->rq_next;

        fsa_free(&_ktask_fsa, old);

        unlock_ktask_rq();

        lock_cores_threads();

        cores_threads[curr_core] = &ktask_thread_placeholder;

        unlock_cores_threads();

        asm volatile("sti");

        // Execute the kernel task
        func(arg);

        asm volatile("cli");

        lock_cores_threads();

        cores_threads[curr_core] = NULL;

        unlock_cores_threads();
    } else {
        unlock_ktask_rq();
    }

    // Pick the thread and mark it RUNNING under the lock, and keep using the local copy afterwards. Otherwise a
    // concurrent kill_process_threads() could mark it DEAD and clear cores_threads[] between the unlock and our
    // write, and we would overwrite DEAD with RUNNING and resume a thread that is being freed.
    lock_cores_threads();
    lock_thread_rq();
    thread_t *thread         = thread_rq_pop();
    cores_threads[curr_core] = thread;
    if (thread) {
        thread->state = RUNNING;
    }
    unlock_thread_rq();
    unlock_cores_threads();

    if (thread) {
        printf("Starting thread %p on core %d\n", thread->tid, curr_core);
        // The thread's regs are the first item in the struct
        // Check if kernel page table needs to be flushed
        if (get_physaddr((uint64_t)&thread->regs, kernel_pml4) == 0x0) {
            ipi_tlb_flush_routine(NULL, NULL);
        }

        thread->base->processor_id     = curr_core;
        thread->base->stack            = (uint64_t)thread->regs.iret_rsp;
        thread->base->cr3              = virt_to_phys((uint64_t)thread->parent->pml4) & ~0xFFF;
        thread->base->proc             = (uint64_t)thread->parent;
        thread->base->kernel_pml4_phys = (uint64_t)kernel_pml4_phys;

        // For syscalls
        cpu_cores[curr_core].kernel_gs_base->proc = (uint64_t)thread->parent;

        // The thread's regs are the first item in the struct
        printf("Thread info: regs addr %p, regs phys addr %p %p, stack %p, proc %p\n", &thread->regs,
               get_physaddr((uint64_t)&thread->regs, thread->parent->pml4),
               get_physaddr((uint64_t)&thread->regs, kernel_pml4), thread->regs.iret_rsp, thread->parent);

        write_msr(IA32_GS_BASE, (uint64_t)thread->base);

        _finalize_task_switch((registers_t *)&thread->regs);
    }

    asm volatile("sti");
    asm volatile("hlt");

    cpu_scheduler_task();
}

int delegate_kernel_task(kernel_task_func_t func, void *arg) {
    // This blocks interrupts and can also be called from an IRQ handler, so we should keep it as fast as possible

    bool ints = are_interrupts_enabled();
    if (ints) {
        asm volatile("cli");
    }

    int res             = 0;
    kernel_task_t *task = (kernel_task_t *)fsa_alloc(&_ktask_fsa);

    if (!task) {
        res = -ENOMEM;
        goto cleanup;
    }

    task->rq_next = NULL;
    task->func    = func;
    task->arg     = arg;

    lock_ktask_rq();

    kernel_task_t **link = &ktask_rq;

    while (*link) {
        link = &((*link)->rq_next);
    }

    *link = task;

    unlock_ktask_rq();

cleanup:
    if (ints) {
        asm volatile("sti");
    }

    send_wakeup_ipi();

    return res;
}