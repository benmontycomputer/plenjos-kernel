#pragma once

#include "proc/scheduler.h"

#include <stdatomic.h>
#include <stdint.h>

#define TIMER_MAX_TIMEOUTS 1024

struct timer_timeout;
typedef void (*timer_callback_func_t)(struct timer_timeout *timeout);

struct timer_timeout {
    _Atomic(uint64_t) milliseconds; // 0 = unused slot; UINT64_MAX = already triggered
    uint64_t start_time;
    timer_callback_func_t callback; // If this is NULL, we treat this as a call to send an IPI wakeup to core_id instead
    union {
        void *data;
        uint64_t core_id;
    };
} __attribute__((packed));

extern struct timer_timeout timer_timeouts[TIMER_MAX_TIMEOUTS];

int set_core_wakeup_timeout(uint64_t milliseconds, uint32_t core_id, uint64_t *started_at_out);

// Max milliseconds is UINT64_MAX - 1
int set_timeout(uint64_t milliseconds, timer_callback_func_t callback, void *data);
int cancel_timeout(int timeout_id);

uint64_t ksleep_blocking_ms(uint64_t milliseconds, uint32_t *curr_core_if_known);
// This doesn't require curr_core_if_known because it may not resume on the same core it started on.
uint64_t ksleep_nonblocking_ms(uint64_t milliseconds, uint32_t priority);