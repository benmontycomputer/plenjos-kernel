#include <stdint.h>

#include "arch/x86_64/gdt/gdt.h"

#include "arch/x86_64/gdt/tss.h"

#include "cpu/cpu.h"

#include <lib/stdio.h>

extern void reload_segments();

//https://github.com/dreamportdev/Osdev-Notes/blob/master/02_Architecture/04_GDT.md

struct GDTR
{
    uint16_t limit;
    uint64_t address;
} __attribute__((packed));

uint64_t gdt_entries[MAX_CORES][num_gdt_entries];

// granularity(8 Bit)=> Flags(Upper 4 Bit) | Up-Limit(Lower 4 Bit)
// flags = (granularity & 0xF0) >> 4
// up_limit = granularity & 0xF
void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entry_t *entry = ((gdt_entry_t *)&gdt_entries[get_curr_core()][i]);
    entry->limit_low     = (limit & 0xFFFF);        // Set lower 16 bit of limit
    entry->granularity   = (limit >> 16) & 0x0F;    // Set upper 4 bit of limit

    entry->base_low      = (base & 0xFFFF);         // Set lower 16 bit baase
    entry->base_middle   = (base >> 16) & 0xFF;     // Set middle 8 bit base
    entry->base_high     = (base >> 24) & 0xFF;     // Set high 8 bit base

    entry->access        = access;                  // Set  bit access

    entry->granularity  |= gran & 0xF0;             // Set 4 bit Flags
    
}

void gdt_fill() {
    //null descriptor, required to be here.
    gdt_entries[get_curr_core()][0] = 0;

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
    gdt_entries[get_curr_core()][2] = kernel_data << 32;

    uint64_t user_code = kernel_code | (3 << 13);
    gdt_entries[get_curr_core()][3] = user_code;

    uint64_t user_data = kernel_data | (3 << 13);
    gdt_entries[get_curr_core()][4] = user_data;

    gdt_entries[get_curr_core()][1] = kernel_code << 32;
}

extern void tss_reload(uint16_t selector);

void gdt_tss_init() {
    // gdt_fill();
    gdt_set_entry(0, 0, 0, 0, 0);              // Null Descriptor
    gdt_set_entry(1, 0, 0xFFFFF, 0b10011010, 0xA0);   // Kernel Code Descriptor , Selector 0x08
    gdt_set_entry(2, 0, 0xFFFFF, 0b10010010, 0xC0);   // Kernel Data Descriptor , Selector 0x10
    gdt_set_entry(3, 0, 0xFFFFF, 0b11111010, 0xA0);   // User Code Descriptor , Selector 0x18
    gdt_set_entry(4, 0, 0xFFFFF, 0b11110010, 0xC0);   // User Data Descriptor , Selector 0x20
    tss_init();

    struct GDTR example_gdtr =
    {
        limit: num_gdt_entries * sizeof(uint64_t) - 1,
        address: (uint64_t)gdt_entries[get_curr_core()],
    };

    asm volatile("lgdt %0" : : "m"(example_gdtr));
    reload_segments();

    tss_reload(TSS_SELECTOR);

    kout(KERNEL_INFO, "Loaded GDT tables.\n");
}