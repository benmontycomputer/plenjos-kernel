#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "limine.h"

#define MAX_CORES 256

typedef struct cpu_core_data {
    uint8_t status; // bit 0 = online, others are undefined

    uint32_t lapic_id;

    bool online;

    struct limine_mp_info *mp_info;
} cpu_core_data_t;

void load_smp();
uint32_t get_curr_core();
int get_n_cores();