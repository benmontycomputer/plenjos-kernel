#pragma once

#include <stdint.h>

extern uint64_t IOAPIC_ADDR;

void enable_ioapic();

uint32_t ioapic_read(void *ioapicaddr, uint32_t reg);
void ioapic_write(void *ioapicaddr, uint32_t reg, uint32_t value);
void ioapic_route_all_irq(uint8_t lapic_id, uint32_t flags);