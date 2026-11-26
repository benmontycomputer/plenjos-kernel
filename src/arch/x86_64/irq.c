#include "arch/x86_64/irq.h"

#include "arch/x86_64/apic/apic.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "memory/mm.h"

#include <stdint.h>

#define TOTAL_IRQ 97

static void (*routines[TOTAL_IRQ])(registers_t *, void *) = { 0 };
static void *routines_data[TOTAL_IRQ]                     = { 0 };

void irq_handler(registers_t *regs) {
    uint64_t index = regs->int_no - 32;

    void (*handler)(registers_t *r, void *data) = routines[index];

    if (handler) handler(regs, routines_data[index]);
    else apic_send_eoi();
}

void irq_register_routine(int index, void (*routine)(registers_t *r, void *data), void *data) {
    routines[index]      = routine;
    routines_data[index] = data;
}

void irq_unregister_routine(int index) {
    routines[index] = 0;
}