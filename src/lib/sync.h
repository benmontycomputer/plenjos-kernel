#pragma once

#include "lock.h"

typedef struct completion completion_t;
typedef struct wait_queue wait_queue_t;
typedef struct excl_lock excl_lock_t;
typedef struct rwsem rwsem_t;

typedef struct task task_t;

#define WQ_INIT         ((wait_queue_t) { .lock = MUTEX_INIT, .waiting = NULL })
#define EXCL_LOCK_INIT  ((excl_lock_t) { .wq = WQ_INIT, .held = false })
#define COMPLETION_INIT ((completion_t) { .cnt = 0, .wq = WQ_INIT })

// ONLY call from kernel tasks; returns 0 on success or -ETIMEDOUT on timeout
void wait_for_completion(completion_t *c);

// Callable from anywhere; readies the waiting task
void complete(completion_t *c);

struct wait_queue {
    mutex lock;
    task_t *waiting;
};

struct excl_lock {
    wait_queue_t wq;
    bool held;
};

struct completion {
    unsigned cnt;
    wait_queue_t wq;
};

void wq_wake_one(wait_queue_t *wq);
void wq_wake_all(wait_queue_t *wq);
void wq_wake_locked(wait_queue_t *wq, bool all);
void wq_sleep_locked(wait_queue_t *wq);

#define wq_wait_event(wq, cond)                                                                               \
    do {                                                                                                      \
        bool _i = irq_save();                                                                                 \
        mutex_lock(&(wq)->lock);                                                                              \
        while (!(cond)) wq_sleep_locked((wq));                                                                \
        mutex_unlock(&(wq)->lock);                                                                            \
        irq_restore(_i);                                                                                      \
    } while (0) // To give this its own scope and make it a single statement for e.g. braceless if statements

#define wq_wait_event_update(wq, cond, pre, post)                                                             \
    do {                                                                                                      \
        bool _i = irq_save();                                                                                 \
        mutex_lock(&(wq)->lock);                                                                              \
        pre;                                                                                                  \
        while (!(cond)) wq_sleep_locked((wq));                                                                \
        post;                                                                                                 \
        mutex_unlock(&(wq)->lock);                                                                            \
        irq_restore(_i);                                                                                      \
    } while (0) // To give this its own scope and make it a single statement for e.g. braceless if statements

#define wq_signal(wq, update, all)                                                                            \
    do {                                                                                                      \
        bool _i = irq_save();                                                                                 \
        mutex_lock(&(wq)->lock);                                                                              \
        update;                                                                                               \
        wq_wake_locked((wq), (all));                                                                          \
        mutex_unlock(&(wq)->lock);                                                                            \
        irq_restore(_i);                                                                                      \
    } while (0) // To give this its own scope and make it a single statement for e.g. braceless if statements

// Same as wq_signal, but will only wake if the (update) results in 0
#define wq_signal_if_0(wq, update, all)                                                                       \
    do {                                                                                                      \
        bool _i = irq_save();                                                                                 \
        mutex_lock(&(wq)->lock);                                                                              \
        if (update == 0) wq_wake_locked((wq), (all));                                                         \
        mutex_unlock(&(wq)->lock);                                                                            \
        irq_restore(_i);                                                                                      \
    } while (0) // To give this its own scope and make it a single statement for e.g. braceless if statements

void excl_lock_acquire(excl_lock_t *lock);
void excl_lock_release(excl_lock_t *lock);

/**
 * rwsem has wait queues and allows for sleeping, etc. while holding lock. It should only be called from a task context.
 */

struct rwsem {
    wait_queue_t wq; // The lock on this protects every field in this struct
    int readers;
    bool writer;
    int writers_waiting;
};

#define RWSEM_INIT ((rwsem_t) { .readers = 0, .writer = false, .writers_waiting = 0, .wq = WQ_INIT })

void rwsem_acquire_read(rwsem_t *rwsem);
void rwsem_release_read(rwsem_t *rwsem);

void rwsem_acquire_write(rwsem_t *rwsem);
void rwsem_release_write(rwsem_t *rwsem);