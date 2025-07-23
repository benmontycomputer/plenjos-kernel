#include <stdint.h>
#include <stdbool.h>

#include <cpuid.h>
#include "arch/x86_64/cpuid/cpuid.h"

#include "arch/x86_64/apic/apic.h"

#include "devices/kconsole.h"

// osdev wiki and 
// https://github.com/baponkar/KeblaOS/blob/main/kernel/src/arch/interrupt/apic/apic.c

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800

uint32_t LAPIC_BASE = 0xFEE00000;   

/** returns a 'true' value if the CPU supports APIC
 *  and if the local APIC hasn't been disabled in MSRs
 *  note that this requires CPUID to be supported.
 */
bool check_apic() {
   uint32_t eax, edx, unused;
   __get_cpuid(1, &eax, &unused, &unused, &edx);
   return edx & CPUID_FEAT_EDX_APIC;
}

/* Set the physical address for local APIC registers */
void cpu_set_apic_base(uintptr_t apic) {
   uint32_t edx = 0;
   uint32_t eax = (apic & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE;

#ifdef __PHYSICAL_MEMORY_EXTENSION__
   edx = (apic >> 32) & 0x0f;
#endif

   cpuSetMSR(IA32_APIC_BASE_MSR, eax, edx);
}

/**
 * Get the physical address of the APIC registers page
 * make sure you map it to virtual memory ;)
 */
uintptr_t cpu_get_apic_base() {
   uint32_t eax, edx;
   cpuGetMSR(IA32_APIC_BASE_MSR, &eax, &edx);

#ifdef __PHYSICAL_MEMORY_EXTENSION__
   return (eax & 0xfffff000) | ((edx & 0x0f) << 32);
#else
   return (eax & 0xfffff000);
#endif
}

uint32_t cpuReadIoApic(void *ioapicaddr, uint32_t reg)
{
   uint32_t volatile *ioapic = (uint32_t volatile *)ioapicaddr;
   ioapic[0] = (reg & 0xff);
   return ioapic[4];
}

void cpuWriteIoApic(void *ioapicaddr, uint32_t reg, uint32_t value)
{
   uint32_t volatile *ioapic = (uint32_t volatile *)ioapicaddr;
   ioapic[0] = (reg & 0xff);
   ioapic[4] = value;
}

void write_reg(uint32_t reg, uint32_t value) {
    *((volatile uint32_t *)(LAPIC_BASE + reg)) = value;
}

uint32_t ReadRegister(uint32_t reg) {
    return *((volatile uint32_t *)(LAPIC_BASE + reg));
}

bool is_apic_enabled() {
    uint32_t l,h;
    cpuGetMSR(IA32_APIC_BASE_MSR, &l, &h);
    return (((((uint64_t)h)<<32)|((uint64_t)l)) & IA32_APIC_BASE_MSR_ENABLE) != 0;
}

void enable_apic() {
    asm volatile("cli");
    /* Section 11.4.1 of 3rd volume of Intel SDM recommends mapping the base address page as strong uncacheable for correct APIC operation. */
    LAPIC_BASE = cpu_get_apic_base();

    /* Hardware enable the Local APIC if it wasn't enabled */
    cpu_set_apic_base(LAPIC_BASE);

    /* Set the Spurious Interrupt Vector Register bit 8 to start receiving interrupts */
    // write_reg(0xF0, ReadRegister(0xF0) | 0x100);
    write_reg(0xF0, (ReadRegister(0xF0) & 0xFFFFFF00) | 0x100 | 0xFF);

    asm volatile("sti");
}