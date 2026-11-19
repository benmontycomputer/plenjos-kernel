#include "cpu/cpu.h"

#include "arch/x86_64/apic/apic.h"
#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/common.h"
#include "arch/x86_64/cpuid/cpuid.h"
#include "arch/x86_64/gdt/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"
#include "kernel.h"
#include "lib/stdio.h"
#include "memory/detect.h"
#include "memory/mm.h"
#include "proc/scheduler.h"
#include "syscall/syscall.h"
#include "timer/pit.h"

#include <limine.h>
#include <stdint.h>

// TODO: do we want to enable sse/sse2/avx instructions?

__attribute__((used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(0);

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_mp_request mp_request = { .id = LIMINE_MP_REQUEST, .revision = 3 };

volatile struct limine_mp_response *mp_response;

volatile cpu_core_data_t cpu_cores[MAX_CORES] = { 0 };
volatile uint64_t gs_bases[MAX_CORES]         = { 0 };

volatile bool smp_loaded = false;

static gsbase_t *new_kernel_gs_base(uint32_t processor_id) {
    gsbase_t *base = (gsbase_t *)phys_to_virt(find_next_free_frame());
    memset(base, 0, PAGE_LEN);

    base->proc = (uint64_t)NULL;

    // This should NEVER be read from the kernel gs_base; the stack field is only for threads' gs_base.
    // If rsp is set to 0, any push will lead to a GPF and a pop would page fault as 0 is unmapped.
    base->stack = 0;

    base->processor_id     = processor_id;
    base->kernel_pml4_phys = (uint64_t)kernel_pml4_phys;

    return base;
}

void setup_bs_gs_base() {
    cpu_cores[0].kernel_gs_base = new_kernel_gs_base(0);

    if (!mp_request.response) {
        panic("No mp info. Halt!\n");
    }

    mp_response = mp_request.response;

    cpu_cores[0].lapic_id = mp_response->bsp_lapic_id;
    cpu_cores[0].mp_info  = mp_response->cpus[0];

    gs_bases[0] = (uint64_t)cpu_cores[0].kernel_gs_base;

    kout(KERNEL_INFO, "base: %p %p\n", gs_bases[0], cpu_cores[0].kernel_gs_base);

    write_msr(IA32_GS_BASE, 0); // no thread yet; the scheduler sets this to the thread's gsbase page
    write_msr(IA32_KERNEL_GS_BASE, (uint64_t)cpu_cores[0].kernel_gs_base);
}

// We reload the entire TLB on every shootdown because the IPI latency dwarfs the difference between single and full
// shootdown.

void ipi_tlb_shootdown_routine(registers_t *regs, void *data) {
    // kout(KERNEL_INFO, "IPI on core %d\n", get_curr_core());
    uint64_t cr3;
    // Get the current value of CR3 (the base of the PML4 table)
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    // Write the value of CR3 back to itself, which will flush the TLB
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    // apic_send_eoi();
}

void ipi_tlb_flush_routine(registers_t *regs, void *data) {
    // kout(KERNEL_INFO, "IPI on core %d\n", get_curr_core());
    uint64_t cr3;
    // Get the current value of CR3 (the base of the PML4 table)
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    // Write the value of CR3 back to itself, which will flush the TLB
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    // apic_send_eoi();
}

void write_tsc_aux(uint32_t core) {
    write_msr(IA32_TSC_AUX_MSR, core);
}

void setup_other_core(struct limine_mp_info *mp_info) {
    set_cr3_addr(kernel_pml4_phys);

    asm volatile("cli");

    write_tsc_aux(mp_info->processor_id);

    kout(KERNEL_INFO, "Setting up core id %d lapic id %d\n", mp_info->processor_id, mp_info->lapic_id);

    cpu_cores[mp_info->processor_id].kernel_gs_base = new_kernel_gs_base(mp_info->processor_id);
    gs_bases[mp_info->processor_id]                 = (uint64_t)cpu_cores[mp_info->processor_id].kernel_gs_base;

    write_msr(IA32_GS_BASE, 0); // no thread yet; the scheduler sets this to the thread's gsbase page
    write_msr(IA32_KERNEL_GS_BASE, (uint64_t)cpu_cores[mp_info->processor_id].kernel_gs_base);

    cpu_cores[mp_info->processor_id].lapic_id = mp_info->lapic_id;
    cpu_cores[mp_info->processor_id].mp_info  = mp_info;

    gdt_tss_init();
    enable_apic();

    idt_load();

    // hpet_init();
    // pit_init();
    apic_start_timer();

    kout(KERNEL_INFO, "Useable memory on detected core %d: %p\n\n", (int)get_curr_core(), PHYS_MEM_USEABLE_LENGTH);

    syscall_init_cpu();

    cpu_cores[mp_info->processor_id].online = true;

    asm volatile("sti");

    for (;;) {
        asm volatile("hlt");
    }
}

void setup_other_cores() {
    if (!mp_response) {
        panic("No mp info. Can't setup other cores. Halt!\n");
    }

    for (uint32_t i = 1; i < MAX_CORES; i++) {
        gs_bases[i]         = 0;
        cores_threads[i]    = NULL;
        cpu_cores[i].online = false;
    }

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
        panic("No mp info. Can't get CPU count. Halt!\n");
    }

    return (int)mp_response->cpu_count;
}

uint32_t get_curr_core() {
    uint32_t aux;
    asm volatile("rdtscp" : "=c"(aux) : : "rax", "rdx");
    return aux;
}

uint32_t get_curr_lapic_id() {
    return cpu_cores[get_curr_core()].lapic_id;
}

void load_smp() {
    asm volatile("cli");

    kout(KERNEL_INFO, "SMP Info:\n    Core count: %p\n", mp_response->cpu_count);

    asm volatile("sti");

    setup_other_cores();

    smp_loaded = true;

    syscall_init_cpu();

    // TODO: flush all tlbs
}