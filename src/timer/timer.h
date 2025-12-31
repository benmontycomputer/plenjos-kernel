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
    timer_callback_func_t callback;
    void *data;
} __attribute__((packed));

extern struct timer_timeout timer_timeouts[TIMER_MAX_TIMEOUTS];

// Max milliseconds is UINT64_MAX - 1
int set_timeout(uint64_t milliseconds, timer_callback_func_t callback, void *data);
int cancel_timeout(int timeout_id);