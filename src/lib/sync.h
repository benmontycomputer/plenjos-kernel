#include "lock.h"
#include "proc/thread.h"

typedef struct completion completion_t;
typedef struct wait_queue wait_queue_t;
typedef struct excl_lock excl_lock_t;

#define COMPLETION_INIT ((completion_t) { .cnt = 0, .lock = MUTEX_INIT, .waiting = NULL })
#define WQ_INIT         ((wait_queue_t) { .lock = MUTEX_INIT, .waiting = NULL })
#define EXCL_LOCK_INIT  ((excl_lock_t) { .wq = WQ_INIT, .held = false })

struct completion {
    mutex lock;
    unsigned cnt;
    thread_t *waiting;
};

// ONLY call from kernel threads; returns 0 on success or -ETIMEDOUT on timeout
int wait_for_completion(completion_t *c, uint64_t timeout_ms);

// Callable from anywhere; readies the waiting thread
void complete(completion_t *c);

struct wait_queue {
    mutex lock;
    thread_t *waiting;
};

struct excl_lock {
    wait_queue_t wq;
    bool held;
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

#define wq_signal(wq, update, all)                                                                            \
    do {                                                                                                      \
        bool _i = irq_save();                                                                                 \
        mutex_lock(&(wq)->lock);                                                                              \
        update;                                                                                               \
        wq_wake_locked((wq), (all));                                                                          \
        mutex_unlock(&(wq)->lock);                                                                            \
        irq_restore(_i);                                                                                      \
    } while (0) // To give this its own scope and make it a single statement for e.g. braceless if statements

void excl_lock_acquire(excl_lock_t *lock);
void excl_lock_release(excl_lock_t *lock);