#include <stdint.h>

#ifndef _KERNEL_IDT_H
#define _KERNEL_IDT_H

#define IDT_MAX_DESCRIPTORS 129

#include "kernel.h"

extern void *isr_stub_table[];

typedef struct {
	uint16_t	limit;
	uint64_t	base;
} __attribute__((packed)) idtr_t;

void idt_load();
void idt_init();
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);
void exception_handler(registers_t *regs);

#endif