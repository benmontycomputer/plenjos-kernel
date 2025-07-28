#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "limine.h"

typedef struct cpu_core_data {
    uint8_t status; // bit 0 = online, others are undefined

    uint32_t lapic_id;

    struct limine_smp_info *smp_info;
} cpu_core_data_t;

void load_smp();