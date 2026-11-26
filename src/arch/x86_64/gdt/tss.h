#ifndef _KERNEL_x86_64_TSS_H
#define _KERNEL_x86_64_TSS_H

#include "cpu/cpu.h"

#include <stdint.h>

#define KERNEL_STACK_SIZE 0x10000

extern struct tss tss_obj[MAX_CORES];
extern uint64_t sched_stack_top[MAX_CORES];

// Add TSS entry structure
struct tss { // 104 bytes is the minimum size of a TSS
    uint32_t reserved0;
    uint64_t rsp0; // Ring 0 Stack Pointer
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1; // Interrupt Stack Entries
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

void tss_init();

uint64_t alloc_kstack();
void free_kstack(uint64_t stack);

#endif