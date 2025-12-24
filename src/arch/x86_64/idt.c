#include <stdbool.h>
#include <stdint.h>

#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"

#include "lib/stdio.h"

#include "kernel.h"

#include "memory/kmalloc.h"
#include "memory/mm.h"

#include "proc/scheduler.h"

#include "arch/x86_64/cpuid/cpuid.h"

// https://wiki.osdev.org/Interrupts_Tutorial

typedef struct {
    uint16_t isr_low;   // The lower 16 bits of the ISR's address
    uint16_t kernel_cs; // The GDT segment selector that the CPU will load into CS before calling the ISR
    uint8_t ist;        // The IST in the TSS that the CPU will load into RSP; set to zero for now
    uint8_t attributes; // Type and attributes; see the IDT page
    uint16_t isr_mid;   // The higher 16 bits of the lower 32 bits of the ISR's address
    uint32_t isr_high;  // The higher 32 bits of the ISR's address
    uint32_t reserved;  // Set to zero
} __attribute__((packed)) idt_entry_t;

__attribute__((aligned(0x10))) static idt_entry_t idt[256]; // Create an array of IDT entries; aligned for performance

idtr_t idtr;

extern void release_console();

// The below function will print some debug message for page fault
void page_fault_handler(registers_t *regs) {
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
    if (present || reserved) printf(") at address %p\n", faulting_address);
    else
        printf(") at address %p, paddr %p\n", faulting_address,
               get_physaddr(faulting_address, (pml4_t *)phys_to_virt(get_cr3_addr())));

    printf("RIP: %p, RSP: %p, RFLAGS: %p, CR3: %p\n", regs->iret_rip, regs->iret_rsp, regs->iret_rflags, regs->cr3);
    /* uint64_t *rsp = (uint64_t *)regs->iret_rsp;
    printf("Stack (rsp = %p) Contents(First 26) :\n", (uint64_t)rsp);

    for (int i = 0; i < 26; i++) {
        printf("  [%p] = %p\n", (uint64_t)(rsp + i), rsp[i]);
    } */

    // printf("%p\n", get_physaddr(faulting_address), 0, 47);

    // Additional action to handle the page fault could be added here,
    // such as invoking a page allocator or terminating a faulty process.

    // Halt the system to prevent further errors (for now).
    // printf("Halting the system due to page fault.\n");
    // hcf();
}

void gpf_handler(registers_t *regs) {
    printf("%s\n", "gpf error");
    printf("recieved interrupt: %d\n", regs->int_no);
    printf("Error Code: %p\n", regs->err_code);
    printf("CS: %p, RIP : %p\n", regs->iret_cs, regs->iret_rip);

    if ((regs->iret_cs & CS_MASK) == KERNEL_CS) {
        printf("Kernel mode GPF occurred at RIP: %p\n", regs->iret_rip);

        uint64_t *rsp = (uint64_t *)regs->iret_rsp;
        printf("Stack (rsp = %p) Contents(First 26) :\n", (uint64_t)rsp);

        for (int i = 0; i < 26; i++) {
            printf("  [%p] = %p\n", (uint64_t)(rsp + i), rsp[i]);
        }
    } else if ((regs->iret_cs & CS_MASK) == USER_CS) {
        pml4_t *pml4 = (pml4_t *)phys_to_virt(regs->cr3 & ~0xFFF);

        printf("User mode GPF occurred at RIP: %p\n", regs->iret_rip);

        uint64_t *rsp = (uint64_t *)regs->iret_rsp;
        printf("Stack (rsp = %p) Contents(First 26) :\n", (uint64_t)rsp);

        for (int i = 0; i < 26; i++) {
            uint64_t stack_phys_addr = get_physaddr((uint64_t)(rsp + i), pml4);
            if (stack_phys_addr == 0) {
                printf("  [%p] = Invalid Address\n", (uint64_t)(rsp + i));
            } else {
                printf("  [%p] = %p\n", (uint64_t)(rsp + i), *(uint64_t *)phys_to_virt(stack_phys_addr));
            }
        }
    } else {
        printf("GPF occurred in unknown segment: CS: %p\n", regs->iret_cs);
    }

    // debug_error_code(regs->err_code);
    // printf("System Halted!\n");
    // hcf();
}

#define IA32_MCG_CTL     0x0177
#define IA32_MCG_CAP     0x0179
#define IA32_MCG_STATUS  0x017A

// base addresses for per-bank registers
#define IA32_MC_CTL(i)    (0x0400 + ((i) * 4))
#define IA32_MC_STATUS(i) (0x0401 + ((i) * 4))
#define IA32_MC_ADDR(i)   (0x0402 + ((i) * 4))
#define IA32_MC_MISC(i)   (0x0403 + ((i) * 4))

void dump_machine_check_msrs()
{
    uint64_t val;

    val = read_msr(IA32_MCG_CAP);
    unsigned banks = (unsigned)(val & 0xff);

    printf("=== Machine Check Global ===\n");
    printf("IA32_MCG_CTL    (0x177): %p\n", read_msr(IA32_MCG_CTL));
    printf("IA32_MCG_CAP    (0x179): %p  (banks=%d)\n", val, banks);
    printf("IA32_MCG_STATUS (0x17A): %p\n", read_msr(IA32_MCG_STATUS));

    for (unsigned i = 0; i < banks; i++) {
        uint64_t status = read_msr(IA32_MC_STATUS(i));
        if (!(status & (1ULL << 63))) {
            // VAL bit clear: no valid error logged
            continue;
        }
        printf("--- Bank %u ---\n", i);
        printf("  CTL   (0x%03x): %p\n", IA32_MC_CTL(i), read_msr(IA32_MC_CTL(i)));
        printf("  STATUS(0x%03x): %p\n", IA32_MC_STATUS(i), status);

        if (status & (1ULL << 58)) { // ADDRV bit
            printf("  ADDR (0x%03x): %p\n", IA32_MC_ADDR(i), read_msr(IA32_MC_ADDR(i)));
        }
        if (status & (1ULL << 59)) { // MISCV bit
            printf("  MISC (0x%03x): %p\n", IA32_MC_MISC(i), read_msr(IA32_MC_MISC(i)));
        }
    }
}

__attribute__((noreturn)) void exception_handler(registers_t *regs) {
    // asm volatile ("cli; hlt"); // Completely hangs the computer

    uint64_t cr3 = get_cr3_addr();

    if (cr3 != kernel_pml4_phys) {
        set_cr3_addr(kernel_pml4_phys);

        uint64_t regs_phys = get_physaddr((uint64_t)regs, (pml4_t *)phys_to_virt(cr3));
        regs = (registers_t *)phys_to_virt(regs_phys);
    }

    release_console();

    if (regs->int_no == 14) {
        printf("\nPAGE FAULT\n");
        page_fault_handler(regs);
    } else if (regs->int_no == 13) {
        printf("\nGPF ERROR\n");
        gpf_handler(regs);
    } else if (regs->int_no < 32) {
        printf("\nCPU EXCEPTION\n");

        dump_machine_check_msrs();
    } else {
        printf("\nEXCEPTION %p\n", regs->int_no);
    }

    if (regs->iret_cs == KERNEL_CS) {
        printf("Kernel panic! System Halted!\n");
        debug_serial = false;
        printf("Kernel panic! System Halted!\n");
        debug_serial = true;
        // TODO: halt all threads
        hcf();
    } else {
        thread_t *thread = cores_threads[get_curr_core()];
        if (thread) {
            pid_t pid = thread->parent->pid;
            pid_t tid = thread->tid;

            printf("Faulting process: PID %p, TID %p\n", pid, tid);
            printf("Killing offending process...\n");

            process_exit(thread->parent);

            cpu_scheduler_task();
        }
    }
}

void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags) {
    idt_entry_t *descriptor = &idt[vector];

    descriptor->isr_low = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs = KERNEL_CS;
    descriptor->ist = 0;
    descriptor->attributes = flags;
    descriptor->isr_mid = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved = 0;
}

static bool vectors[IDT_MAX_DESCRIPTORS];

void idt_load() {
    asm volatile("lidt %0" : : "m"(idtr)); // load the new IDT
    asm volatile("sti");                   // set the interrupt flag
}

void idt_init() {
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1;

    for (uint8_t vector = 0; vector < IDT_MAX_DESCRIPTORS; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        vectors[vector] = true;
        // printf("Registering interrupt %d.\n", vector);
    }

    // syscall should be callable from all rings, unlike other interrupts
    idt_set_descriptor(128, isr_stub_table[128], 0b11101110);

    idt_load();

    printf("Loaded IDT tables.\n");
}