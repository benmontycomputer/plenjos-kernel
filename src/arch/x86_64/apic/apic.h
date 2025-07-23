#ifndef _KERNEL_APIC_H
#define _KERNEL_APIC_H

bool check_apic();
void cpu_set_apic_base(uintptr_t apic);
uintptr_t cpu_get_apic_base();
void enable_apic();
bool is_apic_enabled();

#endif