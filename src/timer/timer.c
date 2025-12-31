#include "timer.h"

#include "hpet.h"
#include "pit.h"

#include <stdatomic.h>

struct timer_timeout timer_timeouts[TIMER_MAX_TIMEOUTS] = { 0 };

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