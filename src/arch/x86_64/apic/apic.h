#ifndef _KERNEL_APIC_H
#define _KERNEL_APIC_H

#include <stdbool.h>
#include <stdint.h>

extern uint64_t LAPIC_BASE;

bool check_apic();
void cpu_set_apic_base(uint64_t apic);
uint64_t cpu_get_apic_base();
void enable_apic();
bool is_apic_enabled();
void apic_send_eoi();
void apic_start_timer();
uint32_t ReadRegister(uint32_t reg);

#endif