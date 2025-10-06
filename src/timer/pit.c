// https://wiki.osdev.org/Programmable_Interval_Timer

#include "timer/pit.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/irq.h"
#include "arch/x86_64/common.h"

#include "lib/stdio.h"

#include "devices/io/ports.h"

#define PIT_FREQ 1193182     // Hz / 1000
#define PIT_INTERR_FREQ 2000 // hertz

volatile uint64_t pit_count;

void pit_irq(registers_t *regs) {
    if (pit_count >= UINT64_MAX) pit_count = 0;

    pit_count++;
    // printf("pit timer %x\n", pit_count);

    apic_send_eoi();
}

void pit_timer_install() {
    irq_register_routine(0, pit_irq);
    irq_register_routine(2, pit_irq);
}

void pit_timer_remove() {
    irq_unregister_routine(0);
    irq_unregister_routine(2);
}

void set_pit_count(unsigned count) {
    // Set low byte
    outb(0x40, count & 0xFF);          // Low byte
    outb(0x40, (count & 0xFF00) >> 8); // High byte

    return;
}

void pit_init() {
    asm volatile("cli");

    pit_timer_install();

    outb(0x43, 0b00110100); // square wave mode
    set_pit_count(PIT_FREQ / PIT_INTERR_FREQ);

    asm volatile("sti");

    apic_send_eoi();
}

void pit_sleep(uint32_t mills) {
    uint64_t end = pit_count + ((PIT_INTERR_FREQ * mills) / 1000);

    while (pit_count < end) {
        // asm volatile("hlt");
    }
}