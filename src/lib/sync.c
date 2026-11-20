#include "sync.h"

#include "arch/x86_64/common.h"
#include "lib/stdio.h"
#include "proc/scheduler.h"

int wait_for_completion(completion_t *c, uint64_t timeout_ms) {
    bool ints = irq_save();

    thread_t *thread = cores_threads[get_curr_core()];

    if (!thread) {
        panic("wait_for_completion() called in non-thread context!\n");
    }

    mutex_lock(&c->lock);

    if (c->waiting) {
        panic("wait_for_completion(): multiple waiters!\n");
    } else {
        c->waiting = thread;
    }

    while (c->cnt == 0) {
        // TODO: timeout
        block(&c->lock, thread);
    }

    c->cnt--;
    c->waiting = NULL;

    mutex_unlock(&c->lock);

    irq_restore(ints);
}

void complete(completion_t *c) {
    bool ints = irq_save();

    mutex_lock(&c->lock);

    c->cnt++;
    thread_t *t = c->waiting;
    c->waiting  = NULL;
    // skips a thread that was killed while blocked
    if (t && t->state == BLOCKED) {
        lock_thread_rq();
        thread_ready(t);
        unlock_thread_rq();
    }

    mutex_unlock(&c->lock);

    irq_restore(ints);
}