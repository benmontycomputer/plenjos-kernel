#pragma once

#include <stdbool.h>
#include <stdint.h>

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define APIC_EOI 0xB0

#define APIC_LVT_TIMER 0x320
#define APIC_TIMER_INITCNT 0x380
#define APIC_TIMER_CURRCNT 0x390
#define APIC_TIMER_DIV 0x3E0

#define LAPIC_ID_REG 0x20

extern uint64_t LAPIC_BASE;

bool check_apic();
void cpu_set_apic_base(uint64_t apic);
uint64_t cpu_get_apic_base();
void enable_apic();
bool is_apic_enabled();
void apic_send_eoi();
void apic_start_timer();
uint32_t get_lapic_id();
uint32_t ReadRegister(uint32_t reg);
void send_ipi(uint8_t apic_id, uint8_t vector);