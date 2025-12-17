#include "lib/lock.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

void mutex_lock(mutex *m) {
    while (atomic_flag_test_and_set_explicit(m, __ATOMIC_ACQUIRE)) {
        // Wait
        __builtin_ia32_pause();
    }
}

void mutex_unlock(mutex *m) {
    atomic_flag_clear_explicit(m, __ATOMIC_RELEASE);
}

void rw_lock_init(rw_lock_t *lock) {
    atomic_store(&lock->state, 0);
    atomic_store(&lock->writers_waiting, 0);
}

static inline void cpu_relax(void) {
    atomic_signal_fence(memory_order_acquire);
}

void rw_lock_read_lock(rw_lock_t *lock) {
    int v;

    for (;;) {
        v = atomic_load_explicit(&lock->state, memory_order_acquire);

        /* Writer active or writer pending */
        if (v < 0 || atomic_load_explicit(&lock->writers_waiting, memory_order_acquire)) {
            cpu_relax();
            continue;
        }

        /* Try to increment reader count */
        if (atomic_compare_exchange_weak_explicit(&lock->state, &v, v + 1, memory_order_acquire,
                                                  memory_order_relaxed)) {
            return;
        }

        cpu_relax();
    }
}

void rw_lock_read_unlock(rw_lock_t *lock) {
    atomic_fetch_sub_explicit(&lock->state, 1, memory_order_release);
}

void rw_lock_write_lock(rw_lock_t *lock) {
    int expected;

    atomic_fetch_add_explicit(&lock->writers_waiting, 1, memory_order_acquire);

    for (;;) {
        expected = 0;

        /* Try to acquire lock exclusively */
        if (atomic_compare_exchange_weak_explicit(&lock->state, &expected, -1, memory_order_acquire,
                                                  memory_order_relaxed)) {
            break;
        }

        cpu_relax();
    }

    atomic_fetch_sub_explicit(&lock->writers_waiting, 1, memory_order_release);
}

void rw_lock_write_unlock(rw_lock_t *lock) {
    atomic_store_explicit(&lock->state, 0, memory_order_release);
}

bool rw_lock_is_write_locked(rw_lock_t *lock) {
    int v = atomic_load_explicit(&lock->state, memory_order_acquire);
    return v < 0;
}

void rw_lock_upgrade_read_to_write(rw_lock_t *lock) {
    atomic_fetch_add_explicit(&lock->writers_waiting, 1, memory_order_acquire);

    // First, release our read lock
    atomic_fetch_sub_explicit(&lock->state, 1, memory_order_release);

    int expected;

    for (;;) {
        expected = 0;

        /* Try to acquire lock exclusively */
        if (atomic_compare_exchange_weak_explicit(&lock->state, &expected, -1, memory_order_acquire,
                                                  memory_order_relaxed)) {
            break;
        }

        cpu_relax();
    }

    atomic_fetch_sub_explicit(&lock->writers_waiting, 1, memory_order_release);
}

void rw_lock_downgrade_write_to_read(rw_lock_t *lock) {
    atomic_store_explicit(&lock->state, 1, memory_order_release);
}

/* bool rw_lock_try_delete_lock(rw_lock_t *lock) {
    int expected = 0;

    bool res = atomic_compare_exchange_strong_explicit(
        &lock->state, &expected, -1,
        memory_order_acquire,
        memory_order_relaxed);

    if (res) {

    }
}

// This allows us to promote our lock without risk of destruction between
void rw_lock_promote_read_to_write_lock(rw_lock_t *lock) {
    // Pre-reserve write lock
    atomic_store(&lock->writer, 1);
} */