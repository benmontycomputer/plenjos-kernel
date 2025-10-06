#include <stdint.h>
#include <stdbool.h>

#ifndef _KERNEL_ARCH_COMMON_H
#define _KERNEL_ARCH_COMMON_H

bool are_interrupts_enabled();

void init_x86_64();

#endif