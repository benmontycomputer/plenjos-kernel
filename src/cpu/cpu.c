#include <stdint.h>

#define LIMINE_API_REVISION 3

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/common.h"
#include "arch/x86_64/cpuid/cpuid.h"
#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"
#include "arch/x86_64/pic/pic.h"
#include "cpu/cpu.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "limine.h"
#include "memory/detect.h"
#include "memory/mm.h"
#include "proc/scheduler.h"
#include "timer/pit.h"

// TODO: do we want to enable sse/sse2/avx instructions?

__attribute__((used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(0);

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_mp_request mp_request
    = { .id = LIMINE_MP_REQUEST, .revision = 3 };

volatile struct limine_mp_response *mp_response;

volatile cpu_core_data_t cpu_cores[MAX_CORES];
volatile uint64_t gs_bases[MAX_CORES];

volatile bool smp_loaded = false;

static gsbase_t *new_kernel_gs_base(uint32_t processor_id) {
    gsbase_t *base = (gsbase_t *)phys_to_virt(find_next_free_frame());
    memset(base, 0, PAGE_LEN);

    base->proc             = (uint64_t)NULL;
    base->cr3              = kernel_pml4_phys;
    base->stack            = TSS_STACK_ADDR;
    base->processor_id     = processor_id;
    base->kernel_pml4_phys = (uint64_t)kernel_pml4_phys;

    return base;
}

void setup_bs_gs_base() {
    cpu_cores[0].kernel_gs_base = new_kernel_gs_base(0);
    gs_bases[0]                 = (uint64_t)cpu_cores[0].kernel_gs_base;

    printf("base: %p %p\n", gs_bases[0], cpu_cores[0].kernel_gs_base);

    write_msr(IA32_GS_BASE, (uint64_t)cpu_cores[0].kernel_gs_base);
    write_msr(IA32_KERNEL_GS_BASE, (uint64_t)cpu_cores[0].kernel_gs_base);
}

// We reload the entire TLB on every shootdown because the IPI latency dwarfs the difference between single and full
// shootdown.

void ipi_tlb_shootdown_routine(registers_t *regs, void *data) {
    // printf("IPI on core %d\n", get_curr_core());
    uint64_t cr3;
    // Get the current value of CR3 (the base of the PML4 table)
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    // Write the value of CR3 back to itself, which will flush the TLB
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    // apic_send_eoi();
}

void ipi_tlb_flush_routine(registers_t *regs, void *data) {
    // printf("IPI on core %d\n", get_curr_core());
    uint64_t cr3;
    // Get the current value of CR3 (the base of the PML4 table)
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    // Write the value of CR3 back to itself, which will flush the TLB
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    // apic_send_eoi();
}

void ipi_kill_routine(registers_t *regs, void *data) {
    printf("Received kill IPI on core %d, halting...\n", get_curr_core());

    // TODO: actually kill the thread and go to the right address
    /* regs->iret_rip = (uint64_t)cpu_scheduler_task;
    regs->iret_cs = KERNEL_CS;
    regs->iret_ss = KERNEL_DS;
    regs->ds = KERNEL_DS;
    regs->es = KERNEL_DS;
    regs->fs = KERNEL_DS;
    regs->gs = KERNEL_DS;
    regs->iret_rsp = TSS_STACK_ADDR; */
    apic_send_eoi();
    cpu_scheduler_task();
}

void ipi_wakeup_routine(registers_t *regs, void *data) {
    // printf("Received IPI wakeup on core %d\n", get_curr_core());
    apic_send_eoi();
}

void setup_other_core(struct limine_mp_info *mp_info) {
    set_cr3_addr(kernel_pml4_phys);

    asm volatile("cli");

    cpu_cores[mp_info->processor_id].lapic_id = mp_info->lapic_id;
    cpu_cores[mp_info->processor_id].mp_info  = mp_info;

    gdt_tss_init();
    enable_apic();

    idt_load();

    // hpet_init();
    // pit_init();
    // apic_start_timer();

    // irq_register_routine(IPI_TLB_SHOOTDOWN_IRQ, &ipi_tlb_shootdown_routine);
    // irq_register_routine(IPI_TLB_FLUSH_IRQ, &ipi_tlb_flush_routine);

    printf("Setting up core id %d lapic id %d\n", mp_info->processor_id, mp_info->lapic_id);

    cpu_cores[mp_info->processor_id].kernel_gs_base = new_kernel_gs_base(mp_info->processor_id);
    gs_bases[mp_info->processor_id]                 = (uint64_t)cpu_cores[mp_info->processor_id].kernel_gs_base;

    write_msr(IA32_GS_BASE, (uint64_t)cpu_cores[mp_info->processor_id].kernel_gs_base);
    write_msr(IA32_KERNEL_GS_BASE, (uint64_t)cpu_cores[mp_info->processor_id].kernel_gs_base);

    write_msr(IA32_GS_BASE, (uint64_t)cpu_cores[mp_info->processor_id].kernel_gs_base);

    printf("Useable memory on detected core %d: %p\n\n", (int)get_curr_core(), PHYS_MEM_USEABLE_LENGTH);

    cpu_cores[mp_info->processor_id].online = true;

    // irq_register_routine(18, &ipi_routine);

    cpu_scheduler_task();
}

void setup_other_cores() {
    if (!mp_response) {
        printf("No mp info. Can't setup other cores. Halt!\n");
        hcf();
    }

    for (uint32_t i = 1; i < MAX_CORES; i++) {
        gs_bases[i]         = 0;
        cores_threads[i]    = NULL;
        cpu_cores[i].online = false;
    }

    cpu_cores[0].online   = true;
    cpu_cores[0].lapic_id = mp_response->bsp_lapic_id;
    cpu_cores[0].mp_info  = mp_response->cpus[0];

    for (uint64_t i = 1; i < mp_response->cpu_count; i++) {
        struct limine_mp_info *mp_info = mp_response->cpus[i];

        mp_info->extra_argument = (uint64_t)mp_info;
        mp_info->goto_address   = (limine_goto_address)setup_other_core;

        while (!cpu_cores[i].online) {
            // Wait for core to come online
            pit_sleep(5);
        }
    }
}

int get_n_cores() {
    if (!mp_response) {
        printf("No mp info. Can't get CPU count. Halt!\n");
        hcf();
    }

    return (int)mp_response->cpu_count;
}

uint32_t get_curr_core() {
    if (!mp_response) {
        return 0;
    }

    uint32_t lapic_id = ReadRegister(LAPIC_ID_REG) >> 24;

    for (int i = 0; i < MAX_CORES; i++) {
        if (cpu_cores[i].lapic_id == lapic_id) {
            return i;
        }
    }

    return -1; // Should never happen
}

uint32_t get_curr_lapic_id() {
    return ReadRegister(LAPIC_ID_REG) >> 24;
}

void load_smp() {
    asm volatile("cli");

    if (!mp_request.response) {
        printf("No mp info. Halt!\n");
        hcf();
    }

    mp_response = mp_request.response;

    printf("SMP Info:\n    Core count: %p\n", mp_response->cpu_count);

    asm volatile("sti");

    setup_other_cores();

    smp_loaded = true;

    // TODO: flush all tlbs
}