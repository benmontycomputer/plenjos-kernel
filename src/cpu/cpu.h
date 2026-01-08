#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "limine.h"

#include "kernel.h"

#define MAX_CORES 256

typedef struct cpu_core_data {
    volatile uint8_t status; // bit 0 = running, others are undefined

    uint32_t lapic_id;

    volatile bool online;

    volatile gsbase_t *kernel_gs_base;

    struct limine_mp_info *mp_info;
} cpu_core_data_t;

extern volatile cpu_core_data_t cpu_cores[MAX_CORES];
extern volatile uint64_t gs_bases[MAX_CORES];
extern volatile struct limine_mp_response *mp_response;

extern volatile bool smp_loaded;

void setup_bs_gs_base();

void load_smp();

uint32_t get_curr_core();
uint32_t get_curr_lapic_id();

void ipi_tlb_shootdown_routine(registers_t *regs, void *data);
void ipi_tlb_flush_routine(registers_t *regs, void *data);
void ipi_kill_routine(registers_t *regs, void *data);
void ipi_wakeup_routine(registers_t *regs, void *data);

int get_n_cores();