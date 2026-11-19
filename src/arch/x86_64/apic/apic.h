#pragma once

#include <stdbool.h>
#include <stdint.h>

#define IA32_APIC_BASE_MSR        0x1B
#define IA32_APIC_BASE_MSR_BSP    0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define IA32_APIC_BASE_MSR_X2APIC 0x400 // Not currently used
#define IA32_TSC_DEADLINE_MSR     0x6E0
#define IA32_TSC_AUX_MSR          0xC0000103 // Stores current cpu core

#define APIC_LVT_TIMER     0x320
#define APIC_TIMER_INITCNT 0x380
#define APIC_TIMER_CURRCNT 0x390
#define APIC_TIMER_DIV     0x3E0
#define APIC_EOI           0xB0

#define LAPIC_ID_REG 0x20

extern uint64_t LAPIC_BASE;

void cpu_set_apic_base(uint64_t apic);
void enable_apic();
bool is_apic_enabled();
void apic_send_eoi();
void apic_calibrate_timer(); // BSP only
void apic_start_timer();     // All cores

void send_ipi(uint8_t apic_id, uint8_t vector);