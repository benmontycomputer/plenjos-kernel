#pragma once

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

typedef atomic_flag mutex;

#define MUTEX_INIT ATOMIC_FLAG_INIT

void mutex_lock(mutex *m);
void mutex_unlock(mutex *m);
bool mutex_is_locked(mutex *m);

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

// Unlike downgrading, this DOES allow other things to acquire a write lock in between. There is no way around this;
// read locks aren't exclusive, so, if we try to upgrade multiple different ones concurrently, one will have to go
// first, so the other one has to wait until the first one is done, and other write locks can be acquired during this
// upgrade. Because of this, a true upgrade is impossible; the best we can do is preventing new readers (but not
// necessarily writers) from acquiring the lock while we wait.
void rw_lock_upgrade_read_to_write(rw_lock_t *lock);

// This completely bypasses any waiting writers UNLIKE upgrading; use only when necessary
void rw_lock_downgrade_write_to_read(rw_lock_t *lock);