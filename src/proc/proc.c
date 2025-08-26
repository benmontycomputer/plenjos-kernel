#include <stdint.h>

#include "proc/proc.h"
#include "proc/thread.h"

#include "memory/mm.h"
#include "memory/kmalloc.h"

#include "lib/stdio.h"
#include "lib/string.h"

#include "arch/x86_64/gdt/tss.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"

#include "cpu/cpu.h"

proc_t *procs = NULL;

uint64_t next_pid = 0;

typedef struct paging_node_list paging_node_list_t;

struct paging_node_list {
    uint64_t node;
    paging_node_list_t *next;
};

paging_node_list_t *nodes = NULL;
paging_node_list_t *last_node = NULL;

uint64_t *proc_alloc_paging_node() {
    uint64_t *node = alloc_paging_node();

    paging_node_list_t *new_node = kmalloc_heap(sizeof(paging_node_list_t));

    new_node->next = NULL;
    new_node->node = (uint64_t)node;

    if (nodes) {
        last_node->next = new_node;
        last_node = new_node;
    } else {
        nodes = new_node;
        last_node = new_node;
    }

    return node;
}

extern struct tss tss_obj;
extern idtr_t idtr;
extern uint64_t gdt_entries[num_gdt_entries];
extern uint64_t phys_mem_total_frames;
extern uint64_t last_useable_phys_frame;

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
    proc->pml4 = (pml4_t *)phys_to_virt(find_next_free_frame());
    memset(proc->pml4, 0, PAGE_LEN);

    printf("proc pml4 virt: %p\n", proc->pml4);

    // proc->pml4 = kernel_pml4;

    // TODO: check perms on this
    // map_virtual_memory_using_alloc(virt_to_phys((uint64_t)proc->pml4), (uint64_t)proc->pml4, PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_USER, proc_alloc_paging_node, proc->pml4);
    map_virtual_memory_using_alloc(get_physaddr((uint64_t)&tss_obj, kernel_pml4), (uint64_t)&tss_obj, sizeof(struct tss), PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    
    for (uint64_t i = TSS_STACK_ADDR - KERNEL_STACK_SIZE; i < TSS_STACK_ADDR; i += PAGE_LEN) {
        // printf("mapping %p\n", gs_bases[i]);
        map_virtual_memory_using_alloc(get_physaddr(i, kernel_pml4), i, PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    }
    
    // map_virtual_memory_using_alloc(get_physaddr((uint64_t)&idtr, kernel_pml4), (uint64_t)&idtr, sizeof(idtr_t), PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    // map_virtual_memory_using_alloc(get_physaddr((uint64_t)idtr.base, kernel_pml4), (uint64_t)idtr.base, PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    // map_virtual_memory_using_alloc(get_physaddr((uint64_t)isr_stub_table, kernel_pml4), (uint64_t)isr_stub_table, PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    // alloc_virtual_memory((uint64_t)isr_stub_table[128], ALLOCATE_VM_EX, proc->pml4);
    // map_virtual_memory_using_alloc(get_physaddr((uint64_t)isr_stub_table[128], kernel_pml4), (uint64_t)isr_stub_table[128], PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    // map_virtual_memory_using_alloc(get_physaddr((uint64_t)gdt_entries, kernel_pml4), (uint64_t)gdt_entries, sizeof(gdt_entries), PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    // map_virtual_memory_using_alloc(0x1000, phys_to_virt(0x1000), last_useable_phys_frame - 0x1000, PAGE_FLAG_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    map_virtual_memory_using_alloc(kernel_load_phys, kernel_load_virt, 1<<20 /* 2 MiB */, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    map_virtual_memory_using_alloc(virt_to_phys(IOAPIC_ADDR), IOAPIC_ADDR, 4096, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    map_virtual_memory_using_alloc(virt_to_phys(LAPIC_BASE), LAPIC_BASE, 4096, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);

    for (uint32_t i = 0; (i < MAX_CORES) && (gs_bases[i]); i++) {
        printf("mapping %p\n", gs_bases[i]);
        map_virtual_memory_using_alloc(get_physaddr((uint64_t)gs_bases[i], kernel_pml4), (uint64_t)gs_bases[i], PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, proc_alloc_paging_node, proc->pml4);
    }

    /* while (nodes) {
        paging_node_list_t *old_nodes = nodes;

        nodes = NULL;
        last_node = NULL;

        while (old_nodes) {
            map_virtual_memory_using_alloc(virt_to_phys(old_nodes->node), old_nodes->node, PAGE_LEN, PAGE_FLAG_PRESENT | PAGE_FLAG_USER, proc_alloc_paging_node, proc->pml4);

            // printf("%p -> %p\n", old_nodes->node, get_physaddr(old_nodes->node, proc->pml4));

            paging_node_list_t *next_node = old_nodes->next;
            kfree_heap(old_nodes);
            old_nodes = next_node;
        }
    } */

    // proc->pml4 = kernel_pml4;

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