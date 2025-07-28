#include <stdint.h>

#define LIMINE_API_REVISION 3

#include "cpu/cpu.h"

#include "kernel.h"
#include "limine.h"

#include "lib/stdio.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(0);

__attribute__((used, section(".limine_requests"))) //
static volatile struct limine_mp_request mp_request
    = { .id = LIMINE_MP_REQUEST, .revision = 3 };

struct limine_mp_response *mp_response;

void setup_other_cores() {
    if (!mp_response) {
        printf("No mp info. Can't setup other cores. Halt!\n");
        hcf();
    }
}

void load_smp() {
    if (!mp_request.response) {
        printf("No mp info. Halt!\n");
        hcf();
    }

    mp_response = mp_request.response;

    printf("SMP Info:\n    Core count: %p\n", mp_response->cpu_count);
}