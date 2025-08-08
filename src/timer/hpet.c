#include <stdint.h>

#include "timer/hpet.h"

#include "arch/x86_64/acpi/acpi.h"
#include "lib/stdio.h"

#define HPET_FREQ (uint64_t)2000000000000000000

GenericAddressStructure hpet_addr;

int hpet_init() {
    if (!hpet_header_global) {
        printf("Can't initialize HPET timer: no HPET detected.\n");
        return 1;
    }

    printf("HPET status: %x\n", hpet_header_global->legacy_replacement);

    return 0;
}