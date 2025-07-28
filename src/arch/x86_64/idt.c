#include <stdint.h>
#include <stdbool.h>

#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"
#include "arch/x86_64/gdt/gdt.h"

#include "lib/stdio.h"

#include "kernel.h"

#include "memory/kmalloc.h"
#include "memory/mm.h"

// https://wiki.osdev.org/Interrupts_Tutorial

typedef struct {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t	    ist;          // The IST in the TSS that the CPU will load into RSP; set to zero for now
	uint8_t     attributes;   // Type and attributes; see the IDT page
	uint16_t    isr_mid;      // The higher 16 bits of the lower 32 bits of the ISR's address
	uint32_t    isr_high;     // The higher 32 bits of the ISR's address
	uint32_t    reserved;     // Set to zero
} __attribute__((packed)) idt_entry_t;

__attribute__((aligned(0x10))) 
static idt_entry_t idt[256]; // Create an array of IDT entries; aligned for performance

typedef struct {
	uint16_t	limit;
	uint64_t	base;
} __attribute__((packed)) idtr_t;

static idtr_t idtr;

// The below function will print some debug message for page fault 
void page_fault_handler(registers_t *regs)
{
    // A page fault has occurred.
    // Retrieve the faulting address from the CR2 register.
    uint64_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

    // Decode the error code to determine the cause of the page fault.
    int present = !(regs->err_code & 0x1); // Page not present
    int rw = regs->err_code & 0x2;         // Write operation?
    int us = regs->err_code & 0x4;         // Processor was in user mode?
    int reserved = regs->err_code & 0x8;   // Overwritten CPU-reserved bits of page entry?
    int id = regs->err_code & 0x10;        // Caused by an instruction fetch?

    // Output an error message with details about the page fault.
    printf("\nPage fault! ( ");
    if (present) printf("Page not present, ");
    if (rw) printf("Not writable, ");
    if (us) printf("User-mode, ");
    if (reserved) printf("Reserved, ");
    if (id) printf("Instruction fetch, ");
    printf(") at address %p\n", faulting_address);

    // printf("%p\n", get_physaddr(faulting_address), 0, 47);


    // Additional action to handle the page fault could be added here,
    // such as invoking a page allocator or terminating a faulty process.

    // Halt the system to prevent further errors (for now).
    printf("Halting the system due to page fault.\n");
    hcf();
}

__attribute__((noreturn))
void exception_handler(registers_t *regs) {
    // __asm__ volatile ("cli; hlt"); // Completely hangs the computer

    if (regs->int_no == 14) {
        printf("\nPAGE FAULT\n");
        page_fault_handler(regs);
    } else if(regs->int_no == 13) {
        printf("\nGPF ERROR\n");
    } else if(regs->int_no < 32) {
        printf("\nCPU EXCEPTION\n");
     }else {
        printf("\nEXCEPTION %p\n", regs->int_no);
    }
    
    hcf();
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = KERNEL_CS;
    descriptor->ist            = 0;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved       = 0;
}

static bool vectors[IDT_MAX_DESCRIPTORS];

extern void* isr_stub_table[];

void idt_init() {
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1;

    for (uint8_t vector = 0; vector < IDT_MAX_DESCRIPTORS; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        vectors[vector] = true;
        // printf("Registering interrupt %d.\n", vector);
    }

    __asm__ volatile ("lidt %0" : : "m"(idtr)); // load the new IDT
    __asm__ volatile ("sti"); // set the interrupt flag

    printf("Loaded IDT tables.\n");
}