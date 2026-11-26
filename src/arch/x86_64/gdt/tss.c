// https://github.com/baponkar/KeblaOS/blob/main/kernel/src/arch/gdt/tss.c

#include "arch/x86_64/gdt/tss.h"

#include "arch/x86_64/gdt/gdt.h"
#include "cpu/cpu.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "memory/mm.h"

#include <stdatomic.h>
#include <stdint.h>

extern uint64_t gdt_entries[MAX_CORES][num_gdt_entries];

struct tss tss_obj[MAX_CORES];
uint64_t sched_stack_top[MAX_CORES];

uint64_t create_descriptor(uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    uint64_t descriptor;

    // Create the high 32 bit segment
    descriptor  = limit & 0x000F0000; // set limit bits 19:16
    // descriptor |= (flag <<  8) & 0x00F0FF00;         // set type, p, dpl, s, g, d/b, l and avl fields
    descriptor |= (access << 8) & 0x0000FF00;
    descriptor |= (gran << 16) & 0x00F00000;
    descriptor |= (base >> 16) & 0x000000FF; // set base bits 23:16
    descriptor |= base & 0xFF000000;         // set base bits 31:24

    // Shift by 32 to allow for low part of segment
    descriptor <<= 32;

    // Create the low 32 bit segment
    descriptor |= base << 16;         // set base bits 15:0
    descriptor |= limit & 0x0000FFFF; // set limit bits 15:0

    return descriptor;
}

void tss_set_entry(int i, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[get_curr_core()][i] = create_descriptor(base, limit, access, gran);
}

uint64_t alloc_kstack() {
    uint64_t stack = atomic_fetch_add(&TSS_STACK_ADDR, PAGE_LEN * 4 + KERNEL_STACK_SIZE) + PAGE_LEN * 4;

    int res = vmalloc((void *)stack, KERNEL_STACK_SIZE, VMA_RW, NULL, kernel_mm);
    if (res) {
        // TODO: handle gracefully
        panic("Stack memory creation failed with res %d!\n", res);
    }

    // TODO: bounds checking; management of free vmem, etc.

    stack += KERNEL_STACK_SIZE;

    kout(KERNEL_INFO, "TSS stack addr: %p\n", stack);

    return stack;
}

void free_kstack(uint64_t stack) {
    // TODO: IMPORTANT: IMPLEMENT
}

void tss_init() {
    uint64_t stack = alloc_kstack();

    uint32_t curr_core = get_curr_core();

    memset(&tss_obj[curr_core], 0, sizeof(struct tss));

    sched_stack_top[curr_core]     = stack;
    tss_obj[curr_core].rsp0        = stack; // Placeholder until first task runs
    tss_obj[curr_core].iopb_offset = sizeof(struct tss);

    uint64_t tss_base  = (uint64_t)&tss_obj[curr_core];
    uint32_t tss_limit = (uint32_t)(sizeof(struct tss));

    // tss_set_entry(5, tss_base, tss_limit, 0x89, 0x00);

    gdt_tss_desc_t *tss_seg = (gdt_tss_desc_t *)&gdt_entries[curr_core][5];

    tss_seg->limit_0  = tss_limit & 0xFFFF;
    tss_seg->addr_0   = tss_base & 0xFFFF;
    tss_seg->addr_1   = (tss_base & 0xFF0000) >> 16;
    tss_seg->type_0   = 0x89;
    tss_seg->limit_1  = (tss_limit & 0xF0000) >> 16;
    tss_seg->addr_2   = (tss_base & 0xFF000000) >> 24;
    tss_seg->addr_3   = tss_base >> 32;
    tss_seg->reserved = 0;

    kout(KERNEL_INFO, "TSS initialized.\n");
}