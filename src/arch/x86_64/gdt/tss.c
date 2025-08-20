// https://github.com/baponkar/KeblaOS/blob/main/kernel/src/arch/gdt/tss.c

#include <stdint.h>

#include "kernel.h"

#include "lib/stdio.h"

#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/gdt/tss.h"

#include "memory/kmalloc.h"

#define STACK_SIZE 0x1000

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
    uint64_t stack = (uint64_t) kmalloc_heap_aligned(STACK_SIZE);

    if (!stack) {
        // Memory creation failed! TODO: output error
        printf("Stack memory creation failed!\n");
        return;
    }
    memset(&tss_obj, 0, sizeof(struct tss));

    tss_obj.rsp0 = stack + STACK_SIZE;
    tss_obj.iopb_offset = sizeof(struct tss);

    uint64_t tss_base = (uint64_t)&tss_obj;
    uint32_t tss_limit = (uint32_t)(sizeof(struct tss));

    tss_set_entry(5, tss_base, tss_limit, 0x89, 0x00);

    printf("TSS initialized.\n");
}