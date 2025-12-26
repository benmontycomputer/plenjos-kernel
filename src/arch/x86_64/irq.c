#include <stdint.h>

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/irq.h"

#include "memory/mm.h"

#include "kernel.h"

#include "lib/stdio.h"

#define TOTAL_IRQ 97

// TODO: IMPORTANT?: PERFORMANCE: map this into userspace so we can check for routine existence without switching page tables
static void (*routines[TOTAL_IRQ])(registers_t *) = { 0 };

static uint32_t testint = 0;

void irq_handler(registers_t *regs) {
    // printf("inter %p\n", regs->int_no);
    uint64_t cr3_phys = get_cr3_addr();
    if (cr3_phys != kernel_pml4_phys) {
        printf("\nERROR: IRQ from other cr3 %p\n\n", cr3_phys);

        // TODO: panic

        return;
    }

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