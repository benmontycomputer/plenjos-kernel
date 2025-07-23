#include <stdint.h>

#ifndef _KERNEL_IDT_H
#define _KERNEL_IDT_H

#define IDT_MAX_DESCRIPTORS 32

void idt_init(void);
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);
void exception_handler(void);

#endif