#pragma once

#include "proc/proc.h"
#include "proc/thread.h"

void thread_ready(thread_t *thread);
void thread_unready(thread_t *thread);

void assign_thread_to_cpu(thread_t *thread);