#include <stdint.h>

#include "proc/proc.h"
#include "proc/thread.h"

#include "memory/mm.h"
#include "memory/kmalloc.h"

#include "lib/stdio.h"
#include "lib/string.h"

proc_t *procs = NULL;

uint64_t next_pid = 0;

proc_t *create_proc(const char *name) {
    proc_t *proc = (proc_t *)kmalloc_heap(sizeof(proc_t));

    if (!proc) {
        printf("Error allocating memory for proc_t.\n");
        return NULL;
    }

    if (name) strncpy(proc->name, name, PROCESS_THREAD_NAME_LEN);
    else
        proc->name[0] = 0;

    proc->next = NULL;
    proc->pid = next_pid++;
    proc->state = READY;
    proc->threads = NULL;

    // TODO: page table
    proc->pml4 = kernel_pml4;

    if (procs) {
        proc_t *p = procs;

        while (p->next)
            p = p->next;

        p->next = proc;
    } else {
        procs = proc;
    }

    return proc;
}