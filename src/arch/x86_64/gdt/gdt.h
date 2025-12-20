#ifndef _KERNEL_x86_64_GDT_H
#define _KERNEL_x86_64_GDT_H

#define NULL_SELECTOR 0x00
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS 0x18
#define USER_DS 0x20
#define TSS_SELECTOR 0x28

// Masks to get the privilege level from a segment selector
#define CS_MASK ~0x3
#define DS_MASK ~0x3

#define num_gdt_entries 7 // 1x null(64bit) + 2x kernel(64bit) + 2x user(64bit) + 1x TSS(128bit)

union gdt_entry
{   
    struct { // 64-Bit
        uint16_t limit_low;       // Lower 16 bits of the segment limit
        uint16_t base_low;        // Lower 16 bits of the base address
        uint8_t base_middle;      // Next 8 bits of the base address
        uint8_t access;           // Access byte
        uint8_t granularity;      // Flags and upper limit
        uint8_t base_high;        // Next 8 bits of the base address
    };
    struct { // 64-Bit
        uint32_t base_upper;     // Upper 32 bits of the base address (for 64-bit TSS)
        uint32_t reserved;       // Reserved, must be zero
    };
};
typedef union gdt_entry gdt_entry_t;

// https://github.com/austanss/skylight/blob/trunk/kernel/src/cpu/gdt/gdt.h
typedef struct {
    uint16_t    limit_0;
    uint16_t    addr_0;
    uint8_t     addr_1;
    uint8_t     type_0;
    uint8_t     limit_1;
    uint8_t     addr_2;
    uint32_t    addr_3;
    uint32_t    reserved;
} __attribute__((packed)) gdt_tss_desc_t;

void gdt_tss_init();

#endif