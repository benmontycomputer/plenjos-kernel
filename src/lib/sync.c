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

void wq_wake_locked(wait_queue_t *wq, bool all) {
    while (wq->waiting) {
        thread_t *t = wq->waiting;
        wq->waiting = t->rq_next;
        t->rq_next  = NULL;

        // Skip threads killed while waiting
        if (t->state == BLOCKED) {
            lock_thread_rq();
            thread_ready(t);
            unlock_thread_rq();
        }

        if (!all) break;
    }
}

void wq_wake_one(wait_queue_t *wq) {
    bool _i = irq_save();
    mutex_lock(&wq->lock);
    wq_wake_locked(wq, false);
    mutex_unlock(&wq->lock);
    irq_restore(_i);
}

void wq_wake_all(wait_queue_t *wq) {
    bool _i = irq_save();
    mutex_lock(&wq->lock);
    wq_wake_locked(wq, true);
    mutex_unlock(&wq->lock);
    irq_restore(_i);
}

void wq_sleep_locked(wait_queue_t *wq) {
    thread_t *thread = cores_threads[get_curr_core()];
    if (!thread) panic("wait_for_completion() called in non-thread context!\n");

    thread->rq_next = NULL;

    thread_t **waiting = &wq->waiting;
    while (*waiting) waiting = &(*waiting)->rq_next;
    *waiting = thread;

    block(&wq->lock, thread);
}

void excl_lock_acquire(excl_lock_t *lock) {
    bool _i = irq_save();
    mutex_lock(&lock->wq.lock);
    while (lock->held) wq_sleep_locked(&lock->wq);
    lock->held = true;
    mutex_unlock(&lock->wq.lock);
    irq_restore(_i);
}

void excl_lock_release(excl_lock_t *lock) {
    bool _i = irq_save();
    mutex_lock(&lock->wq.lock);
    lock->held = false;
    wq_wake_locked(&lock->wq, false);
    mutex_unlock(&lock->wq.lock);
    irq_restore(_i);
}