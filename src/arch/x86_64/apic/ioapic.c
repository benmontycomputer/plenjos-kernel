#include <stdint.h>

#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/common.h"

#include "memory/mm.h"

#include "lib/stdio.h"

// This is the default addr
uint64_t IOAPIC_ADDR = 0xFEC00000;

#define IOAPICID          0x00
#define IOAPICVER         0x01
#define IOAPICARB         0x02
#define IOAPICREDTBL(n)   (0x10 + 2 * n) // lower-32bits (add +1 for upper 32-bits)

void write_ioapic_register(const uint8_t offset, const uint32_t val) 
{
    /* tell IOREGSEL where we want to write to */
    *(volatile uint32_t*)(IOAPIC_ADDR) = offset;
    /* write the value to IOWIN */
    *(volatile uint32_t*)(IOAPIC_ADDR + 0x10) = val; 
}
 
uint32_t read_ioapic_register(const uint8_t offset)
{
    /* tell IOREGSEL where we want to read from */
    *(volatile uint32_t*)(IOAPIC_ADDR) = offset;
    /* return the data from IOWIN */
    return *(volatile uint32_t*)(IOAPIC_ADDR + 0x10);
}

uint32_t ioapic_read(void *ioapicaddr, uint32_t reg)
{
   uint32_t volatile *ioapic = (uint32_t volatile *)ioapicaddr;
   ioapic[0] = (reg & 0xff);
   return ioapic[4];
}

void ioapic_write(void *ioapicaddr, uint32_t reg, uint32_t value)
{
   uint32_t volatile *ioapic = (uint32_t volatile *)ioapicaddr;
   ioapic[0] = (reg & 0xff);
   ioapic[4] = value;
}

void ioapic_route_irq(uint8_t irq_no, uint8_t apic_id, uint8_t vector_no, uint32_t flags) {
    uint32_t reg_low  = 0x10 + irq_no * 2;
    uint32_t reg_high = reg_low + 1;

    // --- HIGH DWORD: Set destination APIC ID ---
    uint32_t high = read_ioapic_register(reg_high);          // Read existing
    high &= 0x00FFFFFF;                                 // Clear bits 24-31 (destination field)
    high |= ((uint32_t)apic_id) << 24;                  // Set new APIC ID
    write_ioapic_register(reg_high, high);                   // Write back

    // --- LOW DWORD: Set vector number and flags ---
    uint32_t low = vector_no | flags;
    write_ioapic_register(reg_low, low);
}


void ioapic_route_all_irq(uint8_t lapic_id, uint32_t flags) {
    for (int i = 0; i < 20; i++) {
        // Skip mouse interrupts for now
        // if (i == 2) continue;
        ioapic_route_irq(i, lapic_id, 32 + i, flags);
    }
}

void enable_ioapic() {
    outb(0x22, 0x70); // select the IOAPIC indirect register
    outb(0x23, 0x01); // sets the IOAPIC indirect register to 1

    map_virtual_memory(IOAPIC_ADDR, 4096, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE);
    IOAPIC_ADDR = phys_to_virt(IOAPIC_ADDR);
}