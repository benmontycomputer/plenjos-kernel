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
        set_cr3_addr(kernel_pml4_phys);

        void (*handler)(registers_t *r) = routines[regs->int_no - 32];

        if (handler) handler(regs);

        set_cr3_addr(cr3_phys);

        apic_send_eoi();

        return;
    }
    /* if (regs->int_no == 0x22) {
        if (!(testint % 10)) {
            // printf("Inter %p %p\n", regs->int_no, testint);
        }
        testint++;
    } */
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