#include <stdint.h>

#include "arch/x86_64/gdt/gdt.h"

#include "arch/x86_64/gdt/tss.h"

#include <lib/stdio.h>

extern void reload_segments();

//https://github.com/dreamportdev/Osdev-Notes/blob/master/02_Architecture/04_GDT.md

struct GDTR
{
    uint16_t limit;
    uint64_t address;
} __attribute__((packed));

uint64_t gdt_entries[num_gdt_entries];

void gdt_fill() {
    //null descriptor, required to be here.
    gdt_entries[0] = 0;

    uint64_t kernel_code = 0;
    kernel_code |= 0b1011 << 8; //type of selector
    kernel_code |= 1 << 12; //not a system descriptor
    kernel_code |= 0 << 13; //DPL field = 0
    kernel_code |= 1 << 15; //present
    kernel_code |= 1 << 21; //long-mode segment

    uint64_t kernel_data = 0;
    kernel_data |= 0b0011 << 8; //type of selector
    kernel_data |= 1 << 12; //not a system descriptor
    kernel_data |= 0 << 13; //DPL field = 0
    kernel_data |= 1 << 15; //present
    kernel_data |= 1 << 21; //long-mode segment
    gdt_entries[2] = kernel_data << 32;

    uint64_t user_code = kernel_code | (3 << 13);
    gdt_entries[3] = user_code;

    uint64_t user_data = kernel_data | (3 << 13);
    gdt_entries[4] = user_data;

    gdt_entries[1] = kernel_code << 32;
}

void gdt_tss_init() {
    gdt_fill();
    tss_init();

    struct GDTR example_gdtr =
    {
        limit: num_gdt_entries * sizeof(uint64_t) - 1,
        address: (uint64_t)gdt_entries,
    };

    asm("lgdt %0" : : "m"(example_gdtr));

    reload_segments();

    printf("Loaded GDT tables.\n");
}