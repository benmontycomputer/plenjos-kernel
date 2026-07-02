#pragma once

#include "stdint.h"

typedef enum {
    ENDPOINT_MEM = 0x0, // kernelfs device, /dev/null, etc.
    ENDPOINT_KBD = 0x1,
    ENDPOINT_FB = 0x2,

    ENDPOINT_TERM = 0x3, // Both real and pseudo; only terminal slaves get endpoints

    ENDPOINT_DRIVE = 0x4,
} endpoint_driver_type_t;