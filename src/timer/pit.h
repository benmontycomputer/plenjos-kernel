#pragma once

#include <stdint.h>

#define PIT_FREQ        1193182 // Hz / 1000
#define PIT_INTERR_FREQ 2000    // hertz

extern volatile uint64_t pit_count;

void pit_init();
void pit_sleep(uint64_t mills);