// https://wiki.osdev.org/Programmable_Interval_Timer

#include "pit.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/common.h"
#include "arch/x86_64/irq.h"
#include "arch/x86_64/apic/ioapic.h"
#include "devices/io/ports.h"
#include "lib/stdio.h"
#include "timer.h"

volatile uint64_t pit_count;

void pit_timer_callback(void *data) {
    struct timer_timeout *timeout = (struct timer_timeout *)data;

    if (timeout->callback) {
        timeout->callback(timeout);
    }

    // Mark the timeout as unused
    atomic_store(&(timeout->milliseconds), 0);
}

void pit_irq(registers_t *regs) {
    if (pit_count >= UINT64_MAX) pit_count = 0;

    pit_count++;
    // printf("pit timer %x\n", pit_count);

    // TODO: separate timers into PIT and HPET ones (or HPET only?)
    struct timer_timeout *timer;
    for (int i = 0; i < TIMER_MAX_TIMEOUTS; i++) {
        timer       = &timer_timeouts[i];
        uint64_t ms = atomic_load(&(timer->milliseconds));
        if (ms != 0 && ms != UINT64_MAX) {
            uint64_t elapsed = pit_count - timer->start_time;
            if (elapsed >= ms) {
                // Timeout reached
                // Clear the timeout first to avoid re-entrancy issues
                atomic_store(&(timer->milliseconds), UINT64_MAX);
                if (timer->callback == NULL) {
                    // IPI wakeup
                    send_ipi((uint32_t)(timer->core_id), IPI_WAKEUP_IRQ + 32);
                    atomic_store(&(timer->milliseconds), 0);
                } else {
                    delegate_kernel_task(pit_timer_callback, timer);
                }
            }
        }
    }

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

void pit_sleep(uint64_t mills) {
    uint64_t end;

    // Same calculation; potentially avoid multiplication overflow
    // TODO: better way to avoid overflow?
    if (PIT_INTERR_FREQ % 1000) {
        end = pit_count + ((PIT_INTERR_FREQ * mills) / 1000);
    } else {
        end = pit_count + (PIT_INTERR_FREQ / 1000) * mills;
    }

    while (pit_count < end) {
        asm volatile("hlt");
    }
}

void pit_sleep_nohlt(uint64_t mills) {
    uint64_t end;

    // Same calculation; potentially avoid multiplication overflow
    // TODO: better way to avoid overflow?
    if (PIT_INTERR_FREQ % 1000) {
        end = pit_count + ((PIT_INTERR_FREQ * mills) / 1000);
    } else {
        end = pit_count + (PIT_INTERR_FREQ / 1000) * mills;
    }

    while (pit_count < end) {
        asm volatile("pause"); // Use pause instead of hlt because this core doesn't receive interrupts
        // TODO: should we have some mechanism for the interrupt core to send IPI to wake this one instead?
    }
}