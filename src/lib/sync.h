#include "lock.h"
#include "proc/thread.h"

typedef struct completion completion_t;
typedef struct wait_queue wait_queue_t;

#define COMPLETION_INIT ((completion_t) { .cnt = 0, .lock = MUTEX_INIT, .waiting = NULL })

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

void wq_wake_one(wait_queue_t *wq);
void wq_wait(wait_queue_t *wq);