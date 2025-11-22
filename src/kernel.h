#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef _KERNEL_H
#define _KERNEL_H

extern bool debug_serial;

void *memcpy(void *dest, const void *src, size_t n);

void *memset(void *s, int c, size_t n);

void *memmove(void *dest, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

void hcf();

extern uint64_t kernel_load_phys;
extern uint64_t kernel_load_virt;

#define IA32_KERNEL_GS_BASE 0xC0000102
#define IA32_GS_BASE        0xC0000101

// This is partially from KeblaOS

// cpu state 
struct regs { // Total 30*8 = 240 bytes, 16 bytes aligned
    // Segment registers
    uint64_t gs;            // Offset 8*0 bytes
    uint64_t fs;            // Offset 8*1 bytes
    uint64_t es;            // Offset 8*2 bytes
    uint64_t ds;            // Offset 8*3 bytes

    uint64_t cr0;           // Offset 8*4 bytes, page table pointer
    // cr1 is reserved and will throw an error when accessed
    uint64_t cr2;           // Offset 8*5 bytes, page table pointer
    uint64_t cr3;           // Offset 8*6 bytes, page table pointer
    uint64_t cr4;           // Offset 8*7 bytes, page table pointer

    // General purpose registers
    uint64_t rax;           // Offset 8*8 bytes
    uint64_t rbx;           // Offset 8*9 bytes
    uint64_t rcx;           // Offset 8*10 bytes
    uint64_t rdx;           // Offset 8*11 bytes
    uint64_t rbp;           // Offset 8*12 bytes
    uint64_t rdi;           // Offset 8*13 bytes
    uint64_t rsi;           // Offset 8*14 bytes
    uint64_t r8;            // Offset 8*15 bytes
    uint64_t r9;            // Offset 8*16 bytes
    uint64_t r10;           // Offset 8*17 bytes
    uint64_t r11;           // Offset 8*18 bytes
    uint64_t r12;           // Offset 8*19 bytes
    uint64_t r13;           // Offset 8*20 bytes
    uint64_t r14;           // Offset 8*21 bytes
    uint64_t r15;           // Offset 8*22 bytes

    uint64_t int_no;        // Offset 8*23 bytes
    uint64_t err_code;      // Offset 8*24 bytes

    // the processor pushes the below registers automatically when an interrupt occurs
    uint64_t iret_rip;      // Offset 8*25 bytes, instruction pointer
    uint64_t iret_cs;       // Offset 8*26 bytes, code segment
    uint64_t iret_rflags;   // Offset 8*27 bytes, flags register
    uint64_t iret_rsp;      // Offset 8*28 bytes, stack pointer
    uint64_t iret_ss;       // Offset 8*29 bytes, stack segment
} __attribute__((packed));
typedef struct regs registers_t;

struct gsbase {
    // This must point to the TOP of the stack
    uint64_t stack; // 0x00
    uint64_t proc; // 0x08
    uint64_t cr3; // 0x10
    uint32_t processor_id; // 0x18
    uint32_t reserved0; // 0x1C
    uint64_t reserved[4]; // 0x20 - 0x38
} __attribute__((packed));
typedef struct gsbase gsbase_t;

/* // cpu state 
struct registers { // Total 26*8 = 208 bytes, 16 bytes aligned
    // Segment registers
    uint64_t gs;            // Offset 8*0 bytes
    uint64_t fs;            // Offset 8*1 bytes
    uint64_t es;            // Offset 8*2 bytes
    uint64_t ds;            // Offset 8*3 bytes

    // General purpose registers
    uint64_t rax;           // Offset 8*4 bytes
    uint64_t rbx;           // Offset 8*5 bytes
    uint64_t rcx;           // Offset 8*6 bytes
    uint64_t rdx;           // Offset 8*7 bytes
    uint64_t rbp;           // Offset 8*8 bytes
    uint64_t rdi;           // Offset 8*9 bytes
    uint64_t rsi;           // Offset 8*10 bytes
    uint64_t r8;            // Offset 8*11 bytes
    uint64_t r9;            // Offset 8*12 bytes
    uint64_t r10;           // Offset 8*13 bytes
    uint64_t r11;           // Offset 8*14 bytes
    uint64_t r12;           // Offset 8*15 bytes
    uint64_t r13;           // Offset 8*16 bytes
    uint64_t r14;           // Offset 8*17 bytes
    uint64_t r15;           // Offset 8*18 bytes

    uint64_t int_no;        // Offset 8*19 bytes
    uint64_t err_code;      // Offset 8*20 bytes

    // the processor pushes the below registers automatically when an interrupt occurs
    uint64_t iret_rip;      // Offset 8*21 bytes, instruction pointer
    uint64_t iret_cs;       // Offset 8*22 bytes, code segment
    uint64_t iret_rflags;   // Offset 8*23 bytes, flags register
    uint64_t iret_rsp;      // Offset 8*24 bytes, stack pointer
    uint64_t iret_ss;       // Offset 8*25 bytes, stack segment
} __attribute__((packed));
typedef struct registers registers_t; */

#endif