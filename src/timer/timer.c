#include "timer.h"

#include "hpet.h"
#include "lib/stdio.h"
#include "pit.h"

#include <stdatomic.h>

struct timer_timeout timer_timeouts[TIMER_MAX_TIMEOUTS] = { 0 };

int set_core_wakeup_timeout(uint64_t milliseconds, uint32_t core_id, uint64_t *started_at_out) {
    for (int i = 0; i < TIMER_MAX_TIMEOUTS; i++) {
        uint64_t expected = 0;
        if (atomic_compare_exchange_strong(&(timer_timeouts[i].milliseconds), &expected, UINT64_MAX)) {
            timer_timeouts[i].start_time = pit_count;
            timer_timeouts[i].callback = NULL;
            timer_timeouts[i].core_id  = core_id;
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
        int res = set_core_wakeup_timeout(milliseconds, curr_core, &end);
        if (res < 0) {
            printf("WARN: ksleep_nonblocking_ms: couldn't set core wakeup timeout! Falling back to power-intensive sleeping methods.\n");
            pit_sleep_nohlt(milliseconds);
            return milliseconds;
        }
        end += milliseconds;
        while (pit_count < end) {
            asm volatile("hlt");
        }

        return (pit_count - end + milliseconds);
    }
}