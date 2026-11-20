#include "timer.h"

#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/common.h"
#include "hpet.h"
#include "lib/stdio.h"
#include "lib/sync.h"
#include "memory/kmalloc.h"
#include "pit.h"

#include <stdatomic.h>

struct timer_timeout timer_timeouts[TIMER_MAX_TIMEOUTS] = { 0 };

static void thread_wakeup_callback(struct timer_timeout *timeout) {
    completion_t *data = (completion_t *)timeout->data;

    complete(data);
}

int set_core_wakeup_timeout(uint64_t milliseconds, uint32_t core_id, uint64_t *started_at_out) {
    for (int i = 0; i < TIMER_MAX_TIMEOUTS; i++) {
        uint64_t expected = 0;
        if (atomic_compare_exchange_strong(&(timer_timeouts[i].milliseconds), &expected, UINT64_MAX)) {
            timer_timeouts[i].start_time = pit_count;
            timer_timeouts[i].callback   = NULL;
            timer_timeouts[i].core_id    = core_id;
            atomic_store(&(timer_timeouts[i].milliseconds), milliseconds);
            if (started_at_out) {
                *started_at_out = timer_timeouts[i].start_time;
            }
            return i;
        }
    }
    return -1;
}

int set_timeout(uint64_t milliseconds, timer_callback_func_t callback, void *data) {
    // TODO: make flags for PIT, HPET support

    for (int i = 0; i < TIMER_MAX_TIMEOUTS; i++) {
        uint64_t expected = 0;
        if (atomic_compare_exchange_strong(&(timer_timeouts[i].milliseconds), &expected, UINT64_MAX)) {
            timer_timeouts[i].start_time = pit_count;
            timer_timeouts[i].callback   = callback;
            timer_timeouts[i].data       = data;
            atomic_store(&(timer_timeouts[i].milliseconds), milliseconds);
            return i;
        }
    }
    return -1;
}

// TODO: have some way of actually keeping track of this
#define INTERRUPT_CORE 0

uint64_t ksleep_nonblocking_ms(uint64_t milliseconds, uint32_t priority) {
    // TODO: make this actually non-blocking
    uint32_t curr_core = get_curr_core();

    if (curr_core == (uint32_t)-1) {
        panic("FATAL ERROR: curr_core = -1 in ksleep_nonblocking_ms\n");
    }

    if (curr_core == INTERRUPT_CORE) {
        pit_sleep(milliseconds);
        return milliseconds;
    } else {
        uint64_t end = 0;
        int res      = set_core_wakeup_timeout(milliseconds, curr_core, &end);
        if (res < 0) {
            printf("WARN: ksleep_nonblocking_ms: couldn't set core wakeup timeout! Falling back to power-intensive "
                   "sleeping methods.\n");
            pit_sleep_nohlt(milliseconds);
            return milliseconds;
        }
        end += milliseconds;

        /* bool interrupts_were_enabled = are_interrupts_enabled();
        uint32_t *prev_low_dwords    = NULL;
        if (!interrupts_were_enabled) {
            prev_low_dwords = (uint32_t *)kmalloc_heap(sizeof(uint32_t) * 128);
            if (!prev_low_dwords) {
                kout(KERNEL_SEVERE_FAULT, "OOM Error: unable to allocate memory for timer masks backup\n");
                return -1;
            }
            ioapic_mask_all_except(2, prev_low_dwords, 128);

            asm volatile("sti");
        } */

        while (pit_count < end) {
            asm volatile("hlt");
        }

        /* if (interrupts_were_enabled == 0) {
            asm volatile("cli");
        }

        if (prev_low_dwords) {
            ioapic_restore_masks(prev_low_dwords, 128);
            kfree_heap(prev_low_dwords);
        } */

        return (pit_count - end + milliseconds);
    }
}

void kthread_sleep_finish(struct timer_timeout *timeout) {
    thread_ready_safe((thread_t *)timeout->data);
}

uint64_t kthread_sleep_ms(uint64_t ms) {
    completion_t c = COMPLETION_INIT;

    set_timeout(ms, thread_wakeup_callback, &c);
    wait_for_completion(&c, -1);

    return 0;
}