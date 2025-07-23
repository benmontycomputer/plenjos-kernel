#include <stdint.h>
#include <stdbool.h>

#ifndef _KERNEL_ARCH_COMMON_H
#define _KERNEL_ARCH_COMMON_H

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);
void io_wait(void);
bool are_interrupts_enabled();

void init_x86_64();

#endif