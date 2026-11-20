#ifndef _KERNEL_H
#define _KERNEL_H

#define _KERNEL_CORRUPTION_CHECK_FSA          1
#define _KERNEL_FSCACHE_CHECK_LOCKS           1
#define _KERNEL_TTY_BUFFER_CORRECTNESS_CHECKS 1
#define _KERNEL_EXTRA_CHECKS_FOR_NULL_ARGS    1
#define _KERNEL_ALWAYS_CHECK_DEVICE_TYPE      1

#include "plenjos/types.h"

#define LIMINE_API_REVISION 3

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINKER_OFFS 0x7fff80000000ULL

/**
 * Conventions:
 *  - frames are used when describing *regions* in memory
 *  - pages are used when actually mapping memory
 *  - they are functionally interchangeable, however.
 */

#define PAGE_ALIGN_BITS 12
#define PAGE_LEN        (1 << PAGE_ALIGN_BITS)

/**
 * We only implement 4-level paging, so the 16 most-significant bits are all the same as the 17th most-significant bit.
 * Thus, we only need 36 bits to represent both physical and virtual frames.
 */
#define VIRT_FRAME_BITS 36
#define PHYS_FRAME_BITS 36

#define CANONICAL_UNIFORM_REGION 0xFFFF000000000000
#define CANONICAL_CHECK_BIT      0x0000800000000000

#define SCHEDULER_TICK_HZ 250

/**
 * This file contains various kernel-wide definitions and structures, as well as config options.
 */

#include "devices/manager.h"

typedef unsigned long ptr_t;

// For easier code readability
typedef ptr_t physptr_t;
typedef ptr_t frame_t; // Specifically for addresses that are bitshifted

/**
 * phys and virt will need separate functions if we ever implement physical or virtual addressing >48bit
 */
inline ptr_t frame_to_addr(frame_t frame) {
    ptr_t res = (frame << PAGE_ALIGN_BITS);
    if (res & CANONICAL_CHECK_BIT) {
        res |= CANONICAL_UNIFORM_REGION;
    } else {
        res &= ~CANONICAL_UNIFORM_REGION;
    }
    return res;
}

inline physptr_t addr_to_frame(frame_t frame) {
    physptr_t res = (frame << PAGE_ALIGN_BITS);
    if (res & CANONICAL_CHECK_BIT) {
        res |= CANONICAL_UNIFORM_REGION;
    } else {
        res &= ~CANONICAL_UNIFORM_REGION;
    }
    return res;
}

extern bool debug_serial;

void *memcpy(void *dest, const void *src, size_t n);

void *memset(void *s, int c, size_t n);

void *memmove(void *dest, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

void hcf();
_Noreturn void panic_nomsg();

static inline size_t min(size_t a, size_t b) {
    return (a > b) ? b : a;
}

extern uint64_t kernel_load_phys;
extern uint64_t kernel_load_virt;

#define IA32_KERNEL_GS_BASE 0xC0000102
#define IA32_GS_BASE        0xC0000101

// This is partially from KeblaOS

// cpu state
struct regs { // Total 30*8 = 240 bytes, 16 bytes aligned
    // Segment registers
    uint64_t gs; // Offset 8*0 bytes
    uint64_t fs; // Offset 8*1 bytes
    uint64_t es; // Offset 8*2 bytes
    uint64_t ds; // Offset 8*3 bytes

    uint64_t cr0; // Offset 8*4 bytes, page table pointer
    // cr1 is reserved and will throw an error when accessed
    uint64_t cr2; // Offset 8*5 bytes, page table pointer
    uint64_t cr3; // Offset 8*6 bytes, page table pointer
    uint64_t cr4; // Offset 8*7 bytes, page table pointer

    // General purpose registers
    uint64_t rax; // Offset 8*8 bytes
    uint64_t rbx; // Offset 8*9 bytes
    uint64_t rcx; // Offset 8*10 bytes
    uint64_t rdx; // Offset 8*11 bytes
    uint64_t rbp; // Offset 8*12 bytes
    uint64_t rdi; // Offset 8*13 bytes
    uint64_t rsi; // Offset 8*14 bytes
    uint64_t r8;  // Offset 8*15 bytes
    uint64_t r9;  // Offset 8*16 bytes
    uint64_t r10; // Offset 8*17 bytes
    uint64_t r11; // Offset 8*18 bytes
    uint64_t r12; // Offset 8*19 bytes
    uint64_t r13; // Offset 8*20 bytes
    uint64_t r14; // Offset 8*21 bytes
    uint64_t r15; // Offset 8*22 bytes

    uint64_t int_no;   // Offset 8*23 bytes
    uint64_t err_code; // Offset 8*24 bytes

    // the processor pushes the below registers automatically when an interrupt occurs
    uint64_t iret_rip;    // Offset 8*25 bytes, instruction pointer
    uint64_t iret_cs;     // Offset 8*26 bytes, code segment
    uint64_t iret_rflags; // Offset 8*27 bytes, flags register
    uint64_t iret_rsp;    // Offset 8*28 bytes, stack pointer
    uint64_t iret_ss;     // Offset 8*29 bytes, stack segment
} __attribute__((packed));
typedef struct regs registers_t;

struct gsbase {
    // This must point to the TOP of the stack
    uint64_t stack;            // 0x00; UNUSED
    uint64_t proc;             // 0x08; UNUSED / BROKEN (DEPRECATED)
    uint64_t syscall_stack;    // 0x10; stores the KERNEL stack for the current USER thread's syscalls
    uint32_t processor_id;     // 0x18
    uint32_t reserved0;        // 0x1C
    uint64_t kernel_pml4_phys; // 0x20
    uint64_t scratch_rax;      // 0x28
    uint64_t scratch_rsp;      // 0x30
    uint64_t scratch_cr3;      // 0x38
} __attribute__((packed));
typedef struct gsbase gsbase_t;

typedef uint64_t (*hash64_func_t)(void *data);
typedef const char *(*key_func_t)(void *data);
typedef int (*compare_func_t)(void *a, void *b, void *data);

inline char hex_to_char(uint8_t val) {
    if (val > 15) return '?';
    if (val < 10) return '0' + val;
    return 'A' + (val - 10);
}

inline uint8_t char_to_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0xFF; // Invalid
}

void populate_string_hexnum(char *buf, size_t len, const char *str, uint32_t hex);

#endif