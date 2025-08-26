#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "limine.h"

#include "kernel.h"

#define MAX_CORES 256

typedef struct cpu_core_data {
    uint8_t status; // bit 0 = online, others are undefined

    uint32_t lapic_id;

    bool online;

    gsbase_t *kernel_gs_base;

    struct limine_mp_info *mp_info;
} cpu_core_data_t;

extern cpu_core_data_t cpu_cores[MAX_CORES];
extern uint64_t gs_bases[MAX_CORES];

void setup_bs_gs_base();

void load_smp();
uint32_t get_curr_core();
int get_n_cores();