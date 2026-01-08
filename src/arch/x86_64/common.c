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

#include "arch/x86_64/irq.h"

#include "arch/x86_64/cpuid/cpuid.h"

#include "memory/detect.h"

#include "timer/hpet.h"
#include "timer/pit.h"

#include "cpu/cpu.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"

void init_x86_64() {
    asm volatile("cli");

    gdt_tss_init();
    acpi_init();

    enable_apic();
    enable_ioapic();

    uint32_t bsp_flags = (0 << 8) | (0 << 13) | (0 << 15); // Flags for the BSP
    ioapic_route_all_irq(get_lapic_id(), bsp_flags);

    idt_init();

    are_interrupts_enabled() ? printf("Interrupts confirmed enabled.\n")
                             : printf("Couldn't confirm interrupts enabled.\n");

    is_apic_enabled() ? printf("APIC confirmed enabled.\n") : printf("Couldn't confirm APIC enabled.\n");

    // hpet_init();
    pit_init();
    // apic_start_timer();

    PIC_remap(PIC1, PIC2);
    pic_disable();

    irq_register_routine(IPI_TLB_SHOOTDOWN_IRQ, &ipi_tlb_shootdown_routine, NULL);
    irq_register_routine(IPI_TLB_FLUSH_IRQ, &ipi_tlb_flush_routine, NULL);
    irq_register_routine(IPI_KILL_IRQ, &ipi_kill_routine, NULL);
    irq_register_routine(IPI_WAKEUP_IRQ, &ipi_wakeup_routine, NULL);

    asm volatile("sti");

    setup_bs_gs_base();

    // ReadRegister(0x60);
}

bool are_interrupts_enabled() {
    unsigned long flags;
    asm volatile("pushf\n\t"
                 "pop %0"
                 : "=g"(flags));
    return flags & (1 << 9);
}