#include <stdbool.h>
#include <stdint.h>

#include "arch/x86_64/cpuid/cpuid.h"
#include <cpuid.h>

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"

#include "arch/x86_64/acpi/acpi.h"

#include "lib/stdio.h"

#include "memory/mm.h"
#include "timer/pit.h"

#include "arch/x86_64/irq.h"

// osdev wiki and
// https://github.com/baponkar/KeblaOS/blob/main/kernel/src/arch/interrupt/apic/apic.c

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define APIC_EOI 0xB0

#define APIC_LVT_TIMER 0x320
#define APIC_TIMER_INITCNT 0x380
#define APIC_TIMER_CURRCNT 0x390
#define APIC_TIMER_DIV 0x3E0

#define LAPIC_ID_REG 0x20

uint64_t LAPIC_BASE = 0x0; // = 0xFEE00000;

/** returns a 'true' value if the CPU supports APIC
 *  and if the local APIC hasn't been disabled in MSRs
 *  note that this requires CPUID to be supported.
 */
bool check_apic() {
    uint32_t eax, edx = 0, unused;
    __get_cpuid(1, &eax, &unused, &unused, &edx);
    return edx & CPUID_FEAT_EDX_APIC;
}

/**
 * Get the physical address of the APIC registers page
 * make sure you map it to virtual memory ;)
 */
uint64_t cpu_get_apic_base() {
    return read_msr(IA32_APIC_BASE_MSR) & 0xFFFFF000;
}

void write_reg(uint32_t reg, uint32_t value) {
    *((volatile uint32_t *)(LAPIC_BASE + (uint64_t)reg)) = value;
}

void apic_send_eoi() {
    write_reg(APIC_EOI, 0);
}

uint32_t ReadRegister(uint32_t reg) {
    return *((volatile uint32_t *)(LAPIC_BASE + (uint64_t)reg));
}

bool is_apic_enabled() {
    uint64_t res = read_msr(IA32_APIC_BASE_MSR);
    return (res & IA32_APIC_BASE_MSR_ENABLE) != 0;
}

uint64_t apic_timer_tpms;

void apic_calibrate_timer() {
    write_reg(APIC_TIMER_DIV, 0b11);

    write_reg(APIC_TIMER_INITCNT, 0xFFFFFFFF);

    pit_sleep(1000);

    // Figure out how many APIC timer ticks have passed
    uint32_t apic_ticks = ReadRegister(APIC_TIMER_CURRCNT);
    uint64_t apic_tmr_freq = (uint64_t)(0xFFFFFFFF - apic_ticks);
    apic_timer_tpms = apic_tmr_freq / 1000;
}

void apic_timer_irq_handler(registers_t *regs) {
    // TODO: implement this
}

void apic_start_timer() {
    irq_register_routine(16, apic_timer_irq_handler);

    apic_calibrate_timer();

    write_reg(APIC_LVT_TIMER, 48 | 0x20000);
    write_reg(APIC_TIMER_DIV, 0b111);
    write_reg(APIC_TIMER_INITCNT, apic_timer_tpms);
}

uint32_t get_lapic_id() {
    return ReadRegister(LAPIC_ID_REG) >> 24;
}

void enable_apic() {
    /* Section 11.4.1 of 3rd volume of Intel SDM recommends mapping the base address page as strong uncacheable for
     * correct APIC operation. */
    // This is mapped in acpi.c

    /* Hardware enable the Local APIC if it wasn't enabled */
    if (!is_apic_enabled()) write_reg(IA32_APIC_BASE_MSR, LAPIC_BASE | IA32_APIC_BASE_MSR_ENABLE);

    /* Set the Spurious Interrupt Vector Register bit 8 to start receiving interrupts */
    // write_reg(0xF0, ReadRegister(0xF0) | 0x100);
    // ReadRegister(0xF0);
    // return;

    write_reg(0xF0, ReadRegister(0xF0) | 0x100);
    // write_reg(0xF0, (ReadRegister(0xF0) & 0xFFFFFF00) | 0x100 | 0xFF);

    apic_send_eoi();

    enable_ioapic();

    uint32_t bsp_flags = (0 << 8) | (0 << 13) | (0 << 15); // Flags for the BSP
    ioapic_route_all_irq(get_lapic_id(), bsp_flags);
}