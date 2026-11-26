#include "proc/scheduler.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/common.h"
#include "arch/x86_64/gdt/tss.h"
#include "arch/x86_64/irq.h"
#include "cpu/cpu.h"
#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/stdio.h"
#include "memory/mm.h"
#include "proc/task.h"
#include "syscall/syscall.h"
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

task_t *current[MAX_CORES];

static task_t ktask_placeholder = { 0 };
static task_t ktask             = { 0 };

task_t *rq              = NULL;
kernel_task_t *ktask_rq = NULL;

static mutex current_lock  = MUTEX_INIT;
static mutex rq_lock       = MUTEX_INIT;
static mutex ktask_rq_lock = MUTEX_INIT;

// Set by kill_process_threads() for a remote core that is still running
static _Atomic bool kill_pending[MAX_CORES];

static uint64_t volatile scheduler_ticks;

// How long kill_process_threads() waits for a remote core to acknowledge before giving up and warning
#define KILL_ACK_SPIN_LIMIT 200000000ULL

extern void kcontext_block(uint64_t *saved_rsp, mutex *m);

__attribute__((noreturn)) //
extern void kcontext_resume(uint64_t saved_rsp);

void lock_current() {
    mutex_lock(&current_lock);
}

void unlock_current() {
    mutex_unlock(&current_lock);
}

void lock_rq() {
    mutex_lock(&rq_lock);
}

void unlock_rq() {
    mutex_unlock(&rq_lock);
}

void lock_ktask_rq() {
    mutex_lock(&ktask_rq_lock);
}

void unlock_ktask_rq() {
    mutex_unlock(&ktask_rq_lock);
}

task_t *rq_pop() {
    if (!rq) {
        return NULL;
    }

    task_t *res = rq;

    rq          = rq->q_next;
    res->q_next = NULL;

    return res;
}

task_t *rq_remove(task_t *task) {
    if (!rq) {
        return NULL;
    }

    task_t **link = &rq;

    while (*link && *link != task) {
        link = &((*link)->q_next);
    }

    if (*link) {
        *link = (*link)->q_next;

        task->q_next = NULL;

        return task;
    } else {
        // Shouldn't happen
        return NULL;
    }
}

void rq_add(task_t *task) {
    task->q_next = NULL;

    task_t **link = &rq;

    while (*link) {
        link = &((*link)->q_next);
    }

    *link = task;
}

// NOTE: caller must still call cpu_scheduler_task(); this function does not IPI the current core as the IPI would be
// received later on and interrupt something else.
void kill_task(task_t *task) {
    // TODO: implement
    panic("kill_task() not implemented!\n");
}

// NOTE: this just forces a schedule call; it does NOT update thread state
// It only does anything if kill_process_threads() flagged this core; otherwise it keeps running
void ipi_kill_routine(registers_t *regs, void *data) {
    // TODO: is this all corrent?
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
// TODO: rename this to yield
void ipi_wakeup_routine(registers_t *regs, void *data) {
    apic_send_eoi();

    uint32_t curr_core = get_curr_core();

    lock_current();

    // Check again w/ lock acquired
    if (current[curr_core] == &ktask_placeholder || current[curr_core] == &ktask) {
        unlock_current();
        return;
    }

    if (current[curr_core]) {
        // Don't preempt kernel code
        // TODO: get rid of this
        if ((regs->iret_cs & 3) == 0) {
            unlock_current();
            return;
        }

        task_t *task       = current[curr_core];
        current[curr_core] = NULL;
        unlock_current();

        lock_rq();
        if (task->state == RUNNING) {
            task->state = READY;
            rq_add(task);
        }
        kcontext_block(&task->thread_state.rsp, &rq_lock);
    } else {
        unlock_current();

        cpu_scheduler_task();
    }
}

void syscall_dispatch(registers_t *regs) {
    uint32_t curr_core = get_curr_core();

    lock_current();

    task_t *task = current[curr_core];
    if (!task || task == &ktask) {
        for (int i = 0; i < get_n_cores(); i++) {
            printf("core %d: task=%p tid=%d state=%d\n", i, current[i], current[i] ? current[i]->tid : -1ULL,
                   current[i] ? current[i]->state : -1);
        }
        panic("syscall_dispatch() core %u (lapic %u) no thread; rax=%lu rip=%p rsp=%p\n", curr_core,
              get_curr_lapic_id() /* or your lapic id read */, regs->rax, regs->iret_rip, regs->iret_rsp);
    }

    unlock_current();

    asm volatile("sti");
    syscall(regs, task);
    asm volatile("cli");
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

void task_ready(task_t *task) {
    if (task->state == READY || task->state == RUNNING) {
        panic("task_ready() called on task %u which was already %s\n", task->tid,
              task->state == RUNNING ? "running" : "ready");
    }
    task->state = READY;
    rq_add(task);

    // TODO: should this be part of scheduler tick or something?
    send_wakeup_ipi();
}

void task_ready_safe(task_t *task) {
    bool _i = irq_save();
    lock_rq();
    task_ready(task);
    unlock_rq();
    irq_restore(_i);
}

void populate_ktask(mm_t *mm) {
    ktask.mm        = NULL;
    ktask.mm_to_use = mm;
    current[0]      = &ktask;
}

void start_scheduler() {
    cpu_cores[0].online = true;
}

void stop_scheduler() {
    for (uint32_t i = 0; i < get_n_cores(); i++) {
        cpu_cores[i].online = false;
        send_ipi(cpu_cores[i].lapic_id, IPI_KILL_IRQ + 32);
    }
}

// Marks the task as blocked and waits for it to be woken, only releasing the mutex at the last second and reacquiring
// it upon wake
void block(wait_queue_t *wq, task_t *task) {
    bool irq = irq_save();
    lock_current();
    task->state              = BLOCKED;
    task->blocked_by         = wq;
    current[get_curr_core()] = NULL;
    unlock_current();

    kcontext_block(&task->thread_state.rsp, &wq->lock);

    mutex_lock(&wq->lock);
    irq_restore(irq);
}

__attribute__((noreturn)) //
void cpu_scheduler_task_c(uint32_t curr_core) {
    // Clear the core's current process and tell kill_process_threads() that this core has left the old thread.
    atomic_store_explicit(&kill_pending[curr_core], false, __ATOMIC_RELEASE);

    if (!cpu_cores[curr_core].online) {
        hcf();
    }

    lock_ktask_rq();
    while (ktask_rq) {
        // There's a kernel task to run
        kernel_task_func_t func = ktask_rq->func;
        void *arg               = ktask_rq->arg;

        kernel_task_t *old = ktask_rq;
        ktask_rq           = ktask_rq->rq_next;

        fsa_free(&_ktask_fsa, old);

        unlock_ktask_rq();

        lock_current();
        current[curr_core] = &ktask_placeholder;
        unlock_current();

        asm volatile("sti");
        // Execute the kernel task. Interrupts are enabled in this function so the interrupt core can still get
        // interrupts that might e.g. schedule ktasks that could then be processed on another core.
        func(arg);
        asm volatile("cli"); // Disable interrupts just in case func() enabled them

        lock_current();
        current[curr_core] = NULL;
        unlock_current();

        lock_ktask_rq();
    }

    unlock_ktask_rq();

    // Pick the thread and mark it RUNNING under the lock, and keep using the local copy afterwards. Otherwise a
    // concurrent kill_process_threads() could mark it DEAD and clear current[] between the unlock and our
    // write, and we would overwrite DEAD with RUNNING and resume a thread that is being freed.
    lock_current();
    lock_rq();
    task_t *task       = rq_pop();
    current[curr_core] = task;
    if (task) {
        task->state = RUNNING;
    }
    unlock_rq();
    unlock_current();

    if (task) {
        // For syscalls
        cpu_cores[curr_core].kernel_gs_base->syscall_stack = (uint64_t)task->kstack_top;
        // For interrupts/exceptions taken from user mode
        tss_obj[curr_core].rsp0                            = (uint64_t)task->kstack_top;

        if (get_cr3_addr() != task->mm_to_use->cr3) {
            set_cr3_addr(task->mm_to_use->cr3);
        }

        kcontext_resume(task->thread_state.rsp);
    }

    for (;;) {
        asm volatile("sti");
        asm volatile("hlt");
        asm volatile("cli");
        if (ktask_rq || rq) {
            cpu_scheduler_task();
        }
    }
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