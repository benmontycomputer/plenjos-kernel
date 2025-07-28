#include <stdbool.h>
#include <stdint.h>

#include <cpuid.h>

#include "lib/stdio.h"

#include "arch/x86_64/common.h"

#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/idt.h"

#include "arch/x86_64/acpi/acpi.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/pic/pic.h"

#include "arch/x86_64/cpuid/cpuid.h"

#include "memory/detect.h"

void init_x86_64() {
    asm volatile("cli");

    gdt_tss_init();
    acpi_init();

    enable_apic();

    idt_init();

    are_interrupts_enabled() ? printf("Interrupts confirmed enabled.\n")
                             : printf("Couldn't confirm interrupts enabled.\n");

    is_apic_enabled() ? printf("APIC confirmed enabled.\n") : printf("Couldn't confirm APIC enabled.\n");

    apic_start_timer();

    PIC_remap(PIC1, PIC2);
    pic_disable();

    asm volatile("sti");

    // ReadRegister(0x60);
}

inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
    /* There's an outb %al, $imm8 encoding, for compile-time constant port
     * numbers that fit in 8b. (N constraint). Wider immediate constants would
     * be truncated at assemble-time (e.g. "i" constraint). The  outb  %al, %dx
     * encoding is the only option for all other cases. %1 expands to %dx
     * because  port  is a uint16_t.  %w1 could be used if we had the port
     * number a wider C type */
}

inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

inline void io_wait(void) {
    outb(0x80, 0);
}

inline bool are_interrupts_enabled() {
    unsigned long flags;
    asm volatile("pushf\n\t"
                 "pop %0"
                 : "=g"(flags));
    return flags & (1 << 9);
}