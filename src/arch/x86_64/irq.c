#include <stdint.h>

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/irq.h"

#include "memory/mm.h"

#include "kernel.h"

#include "lib/stdio.h"

#define TOTAL_IRQ 32

static void (*routines[TOTAL_IRQ])(registers_t *) = { 0 };

void irq_handler(registers_t *regs) {
    // printf("inter %p\n", regs->int_no);
    void (*handler)(registers_t *r) = routines[regs->int_no - 32];

    if (handler) handler(regs);

    apic_send_eoi();
}

void irq_register_routine(int index, void (*routine)(registers_t *r)) {
    routines[index] = routine;
}

void irq_unregister_routine(int index) {
    routines[index] = 0;
}