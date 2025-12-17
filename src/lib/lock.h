#pragma once

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

typedef atomic_flag mutex;

void mutex_lock(mutex *m);
void mutex_unlock(mutex *m);

// This lock allows multiple readers but only one writer (and no readers when writing)
typedef struct rw_lock {
    atomic_int state;
    atomic_int writers_waiting;
} rw_lock_t;

void rw_lock_init(rw_lock_t *lock);
void rw_lock_read_lock(rw_lock_t *lock);
void rw_lock_read_unlock(rw_lock_t *lock);
void rw_lock_write_lock(rw_lock_t *lock);
void rw_lock_write_unlock(rw_lock_t *lock);
bool rw_lock_is_write_locked(rw_lock_t *lock);