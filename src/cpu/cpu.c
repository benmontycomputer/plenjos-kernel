#include <stdint.h>

#define LIMINE_API_REVISION 3

#include "cpu/cpu.h"

#include "kernel.h"
#include "limine.h"

#include "timer/pit.h"

#include "memory/mm.h"
#include "memory/detect.h"

#include "lib/stdio.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(0);

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_mp_request mp_request
    = { .id = LIMINE_MP_REQUEST, .revision = 3 };

struct limine_mp_response *mp_response;

cpu_core_data_t cpu_cores[MAX_CORES];

void setup_other_core(struct limine_mp_info *mp_info) {
    printf("Setting up core id %d\n", mp_info->processor_id);

    cpu_cores[mp_info->processor_id].lapic_id = mp_info->lapic_id;
    cpu_cores[mp_info->processor_id].mp_info = mp_info;

    cpu_cores[mp_info->processor_id].online = true;

    printf("Useable memory: %p\n\n", PHYS_MEM_USEABLE_LENGTH);

    asm volatile("sti");

    for (;;) {
        asm volatile("hlt");
    }
}

void setup_other_cores() {
    if (!mp_response) {
        printf("No mp info. Can't setup other cores. Halt!\n");
        hcf();
    }

    for (uint64_t i = 1; i < mp_response->cpu_count; i++) {
        struct limine_mp_info *mp_info = mp_response->cpus[i];

        mp_info->extra_argument = (uint64_t)mp_info;
        mp_info->goto_address = (limine_goto_address)setup_other_core;
        
        pit_sleep(200);
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
}