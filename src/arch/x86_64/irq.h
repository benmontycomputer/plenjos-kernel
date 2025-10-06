#pragma once

#include "kernel.h"

#define KEYBOARD_IRQ 1
#define SYSCALL_IRQ 96

#define IPI_TLB_SHOOTDOWN_IRQ 18
#define IPI_TLB_FLUSH_IRQ 19
#define IPI_KILL_IRQ 19

void irq_register_routine (int index, void (*routine)(registers_t *r));
void irq_unregister_routine (int index);