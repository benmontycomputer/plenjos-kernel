// https://github.com/baponkar/KeblaOS/blob/main/kernel/src/arch/gdt/tss.c

#include <stdint.h>

#include "kernel.h"

#include "lib/stdio.h"

#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/gdt/tss.h"

#include "memory/mm.h"

extern uint64_t gdt_entries[num_gdt_entries];

struct tss tss_obj;

uint64_t create_descriptor(uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    uint64_t descriptor;
 
    // Create the high 32 bit segment
    descriptor  =  limit       & 0x000F0000;         // set limit bits 19:16
    //descriptor |= (flag <<  8) & 0x00F0FF00;         // set type, p, dpl, s, g, d/b, l and avl fields
    descriptor |= (access << 8) & 0x0000FF00;
    descriptor |= (gran << 16) & 0x00F00000;
    descriptor |= (base >> 16) & 0x000000FF;         // set base bits 23:16
    descriptor |=  base        & 0xFF000000;         // set base bits 31:24
 
    // Shift by 32 to allow for low part of segment
    descriptor <<= 32;
 
    // Create the low 32 bit segment
    descriptor |= base  << 16;                       // set base bits 15:0
    descriptor |= limit  & 0x0000FFFF;               // set limit bits 15:0
 
    return descriptor;
}

void tss_set_entry(int i, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[i] = create_descriptor(base, limit, access, gran);
}

void tss_init() {
    uint64_t stack = TSS_STACK_ADDR;

    for (uint64_t i = 0; i < KERNEL_STACK_SIZE; i += PAGE_LEN) {
        uint64_t frame = find_next_free_frame();
        if (!frame) {
            printf("Stack memory creation failed!\n");
            hcf();
        }
        map_virtual_memory_using_alloc(frame, stack + i, true, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE, alloc_paging_node, kernel_pml4);
    }

    stack += KERNEL_STACK_SIZE;
    TSS_STACK_ADDR += KERNEL_STACK_SIZE;

    printf("TSS stack addr: %p\n", stack);

    memset(&tss_obj, 0, sizeof(struct tss));

    tss_obj.rsp0 = stack;
    tss_obj.iopb_offset = sizeof(struct tss);

    uint64_t tss_base = (uint64_t)&tss_obj;
    uint32_t tss_limit = (uint32_t)(sizeof(struct tss));

    // tss_set_entry(5, tss_base, tss_limit, 0x89, 0x00);

    gdt_tss_desc_t *tss_seg = (gdt_tss_desc_t *)&gdt_entries[5];

    tss_seg->limit_0 = tss_limit & 0xFFFF;
    tss_seg->addr_0 = tss_base & 0xFFFF;
    tss_seg->addr_1 = (tss_base & 0xFF0000) >> 16;
    tss_seg->type_0 = 0x89;
    tss_seg->limit_1 = (tss_limit & 0xF0000) >> 16;
    tss_seg->addr_2 = (tss_base & 0xFF000000) >> 24;
    tss_seg->addr_3 = tss_base >> 32;
    tss_seg->reserved = 0;

    printf("TSS initialized.\n");
}