#include <stdint.h>

#include "proc/proc.h"
#include "proc/thread.h"

#include "memory/mm.h"
#include "memory/kmalloc.h"

#include "lib/stdio.h"
#include "lib/string.h"

proc_t *procs = NULL;

uint64_t next_pid = 0;

uint64_t *nodes = NULL;

uint64_t *proc_alloc_paging_node() {
    uint64_t *node = alloc_paging_node();
}

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
    // proc->pml4 = (pml4_t *)phys_to_virt(find_next_free_frame());
    // map_virtual_memory_using_alloc(virt_to_phys((uint64_t)proc->pml4), (uint64_t)proc->pml4, PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_USER, proc_alloc_paging_node, proc->pml4);

    // memset(proc->pml4, 0, PAGE_LEN);
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