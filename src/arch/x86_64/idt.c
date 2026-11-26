#include "arch/x86_64/idt.h"

#include "arch/x86_64/cpuid/cpuid.h"
#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/irq.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "memory/mm.h"
#include "proc/scheduler.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

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
extern void request_console();
extern int _printf_internal(const char *format, va_list list);

static int kout_panic(kernel_msg_level_t msg_level, const char *format, ...) {
    if (msg_level == KERNEL_PANIC) debug_serial = false;
    va_list list;
    va_start(list, format);

    int res = _printf_internal(format, list);

    va_end(list);

    return 0;
}

// The below function will print some debug message for page fault
void page_fault_handler(registers_t *regs) {
    // A page fault has occurred.
    // Retrieve the faulting address from the CR2 register.
    uint64_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

    // Decode the error code to determine the cause of the page fault.
    int present  = !(regs->err_code & 0x1); // Page not present
    int rw       = regs->err_code & 0x2;    // Write operation?
    int us       = regs->err_code & 0x4;    // Processor was in user mode?
    int reserved = regs->err_code & 0x8;    // Overwritten CPU-reserved bits of page entry?
    int id       = regs->err_code & 0x10;   // Caused by an instruction fetch?

    // Output an error message with details about the page fault.
    kout_panic(KERNEL_PANIC, "\nPage fault! ( ");
    if (present) kout_panic(KERNEL_PANIC, "Page not present, ");
    if (rw) kout_panic(KERNEL_PANIC, "Not writable, ");
    if (us) kout_panic(KERNEL_PANIC, "User-mode, ");
    if (reserved) kout_panic(KERNEL_PANIC, "Reserved, ");
    if (id) kout_panic(KERNEL_PANIC, "Instruction fetch, ");
    if (present || reserved) kout_panic(KERNEL_PANIC, ") at address %p\n", faulting_address);
    else
        kout_panic(KERNEL_PANIC, ") at address %p, paddr %p\n", faulting_address,
                   get_physaddr(faulting_address, current[get_curr_core()]->mm_to_use));

    kout_panic(KERNEL_PANIC, "RIP: %p, RSP: %p, RFLAGS: %p, CR3: %p\n", regs->iret_rip, regs->iret_rsp,
               regs->iret_rflags, get_cr3_addr());
}

void gpf_handler(registers_t *regs) {
    kout_panic(KERNEL_PANIC, "%s\n", "gpf error");
    kout_panic(KERNEL_PANIC, "received interrupt: %d\n", regs->int_no);
    kout_panic(KERNEL_PANIC, "Error Code: %p\n", regs->err_code);
    kout_panic(KERNEL_PANIC, "CS: %p, RIP : %p\n", regs->iret_cs, regs->iret_rip);

    // debug_error_code(regs->err_code);
    // kout_panic(KERNEL_PANIC, "System Halted!\n");
    // hcf();
}

#define IA32_MCG_CTL    0x0177
#define IA32_MCG_CAP    0x0179
#define IA32_MCG_STATUS 0x017A

// base addresses for per-bank registers
#define IA32_MC_CTL(i)    (0x0400 + ((i) * 4))
#define IA32_MC_STATUS(i) (0x0401 + ((i) * 4))
#define IA32_MC_ADDR(i)   (0x0402 + ((i) * 4))
#define IA32_MC_MISC(i)   (0x0403 + ((i) * 4))

void dump_machine_check_msrs() {
    uint64_t val;

    val            = read_msr(IA32_MCG_CAP);
    unsigned banks = (unsigned)(val & 0xff);

    kout_panic(KERNEL_PANIC, "=== Machine Check Global ===\n");
    kout_panic(KERNEL_PANIC, "IA32_MCG_CTL    (0x177): %p\n", read_msr(IA32_MCG_CTL));
    kout_panic(KERNEL_PANIC, "IA32_MCG_CAP    (0x179): %p  (banks=%d)\n", val, banks);
    kout_panic(KERNEL_PANIC, "IA32_MCG_STATUS (0x17A): %p\n", read_msr(IA32_MCG_STATUS));

    for (unsigned i = 0; i < banks; i++) {
        uint64_t status = read_msr(IA32_MC_STATUS(i));
        if (!(status & (1ULL << 63))) {
            // VAL bit clear: no valid error logged
            continue;
        }
        kout_panic(KERNEL_PANIC, "--- Bank %u ---\n", i);
        kout_panic(KERNEL_PANIC, "  CTL   (0x%03x): %p\n", IA32_MC_CTL(i), read_msr(IA32_MC_CTL(i)));
        kout_panic(KERNEL_PANIC, "  STATUS(0x%03x): %p\n", IA32_MC_STATUS(i), status);

        if (status & (1ULL << 58)) { // ADDRV bit
            kout_panic(KERNEL_PANIC, "  ADDR (0x%03x): %p\n", IA32_MC_ADDR(i), read_msr(IA32_MC_ADDR(i)));
        }
        if (status & (1ULL << 59)) { // MISCV bit
            kout_panic(KERNEL_PANIC, "  MISC (0x%03x): %p\n", IA32_MC_MISC(i), read_msr(IA32_MC_MISC(i)));
        }
    }
}

__attribute__((noreturn)) void exception_handler(registers_t *regs) {
    // asm volatile ("cli; hlt"); // Completely hangs the computer

    bool is_user                 = (regs->iret_cs & CS_MASK) == USER_CS;
    kernel_msg_level_t msg_level = (regs->iret_cs != USER_CS) ? KERNEL_PANIC : USER_FAULT;

    // release_console();

    if (!is_user && smp_loaded) {
        stop_scheduler();
    }

    uint32_t curr_core = get_curr_core();

    kout_panic(msg_level, "\nCORE %d", curr_core);

    if (is_user && current[curr_core]) {
        kout_panic(msg_level, ", TID %d, TGID %d", (int)current[curr_core]->tid, (int)current[curr_core]->tgid);
    }

    kout_panic(msg_level, ", CS %x", regs->iret_cs);
    kout_panic(msg_level, ", CR3 %p:", get_cr3_addr());

    if (regs->int_no == 14) {
        kout_panic(msg_level, "\nPAGE FAULT\n");
        page_fault_handler(regs);
    } else if (regs->int_no == 13) {
        kout_panic(msg_level, "\nGPF ERROR\n");
        gpf_handler(regs);
    } else if (regs->int_no < 32) {
        kout_panic(msg_level, "\nCPU EXCEPTION\n");
        kout_panic(msg_level, "RSP: %p\n", regs->iret_rsp);
        kout_panic(msg_level, "RIP: %p\n", regs->iret_rip);
        kout_panic(msg_level, "Error Code: %p\n", regs->err_code);

        dump_machine_check_msrs();
    } else {
        kout_panic(msg_level, "\nEXCEPTION %p\n", regs->int_no);
    }

    if (!is_user) {
        kout_panic(msg_level, "Kernel mode exception occurred at RIP: %p\n", regs->iret_rip);

        uint64_t *rsp = (uint64_t *)regs->iret_rsp;
        kout_panic(msg_level, "Stack (rsp = %p) Contents(First 26) :\n", (uint64_t)rsp);

        for (int i = 0; i < 26; i++) {
            kout_panic(msg_level, "  [%p] = %p\n", (uint64_t)(rsp + i), rsp[i]);
        }
    } else {
        kout_panic(msg_level, "User mode exception occurred at RIP: %p\n", regs->iret_rip);

        uint64_t *rsp = (uint64_t *)regs->iret_rsp;
        kout_panic(msg_level, "Stack (rsp = %p) Contents(First 26) :\n", (uint64_t)rsp);

        for (int i = 0; i < 26; i++) {
            uint64_t stack_phys_addr = get_physaddr((uint64_t)(rsp + i), current[curr_core]->mm_to_use);
            if (stack_phys_addr == 0) {
                kout_panic(msg_level, "  [%p] = Invalid Address\n", (uint64_t)(rsp + i));
            } else {
                kout_panic(msg_level, "  [%p] = %p\n", (uint64_t)(rsp + i), *(uint64_t *)phys_to_virt(stack_phys_addr));
            }
        }
    }

    if (!is_user) {
        panic("Unrecoverable kernel exception! System halted!\n");
    } else {
        task_t *task = current[get_curr_core()];
        if (task) {
            kout_panic(msg_level, "Faulting task: TID %p, TGID %p\n", task->tid, task->tgid);
            kout_panic(msg_level, "Killing offending task...\n");

            task_exit(task);

            cpu_scheduler_task();
        }
    }

    cpu_scheduler_task();
}

void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags) {
    idt_entry_t *descriptor = &idt[vector];

    descriptor->isr_low    = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs  = KERNEL_CS;
    descriptor->ist        = 0;
    descriptor->attributes = flags;
    descriptor->isr_mid    = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high   = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved   = 0;
}

static bool vectors[IDT_MAX_DESCRIPTORS];

void idt_load() {
    asm volatile("lidt %0" : : "m"(idtr)); // load the new IDT
}

void idt_init() {
    idtr.base  = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1;

    for (uint8_t vector = 0; vector < IDT_MAX_DESCRIPTORS; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        vectors[vector] = true;
        // kout(KERNEL_INFO, "Registering interrupt %d.\n", vector);
    }

    idt_load();

    kout(KERNEL_INFO, "Loaded IDT tables.\n");
}