#ifndef _KERNEL_x86_64_GDT_H
#define _KERNEL_x86_64_GDT_H

#define NULL_SELECTOR 0x00
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS 0x18
#define USER_DS 0x20

#define num_gdt_entries 7 // 1x null(64bit) + 2x kernel(64bit) + 2x user(64bit) + 1x TSS(128bit)

void gdt_tss_init();

#endif