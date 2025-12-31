#pragma once

#include "kernel.h"

#define KEYBOARD_IRQ 1
#define SYSCALL_IRQ  96

#define IDE_PRIMARY_IRQ   14
#define IDE_SECONDARY_IRQ 15

#define IPI_TLB_SHOOTDOWN_IRQ 18
#define IPI_TLB_FLUSH_IRQ     19
#define IPI_KILL_IRQ          19
#define IPI_WAKEUP_IRQ        20

void irq_register_routine(int index, void (*routine)(registers_t *r));
void irq_unregister_routine(int index);