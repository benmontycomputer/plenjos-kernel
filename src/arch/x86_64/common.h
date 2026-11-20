#include <stdint.h>
#include <stdbool.h>

#ifndef _KERNEL_ARCH_COMMON_H
#define _KERNEL_ARCH_COMMON_H

bool are_interrupts_enabled();

static inline bool irq_save(void) {
    bool were_on = are_interrupts_enabled();
    asm volatile("cli");
    return were_on;
}
static inline void irq_restore(bool were_on) {
    if (were_on) asm volatile("sti");
}

void init_x86_64();

#endif