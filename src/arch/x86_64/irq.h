#pragma once

#include "kernel.h"

#define KEYBOARD_IRQ 1
#define SYSCALL_IRQ 96

void irq_register_routine (int index, void (*routine)(registers_t *r));
void irq_unregister_routine (int index);