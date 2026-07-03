#pragma once

#include "stdint.h"
#include "plenjos/types.h"

// Major number for devices
typedef enum {
    ENDPOINT_MEM = 0x0, // kernelfs device, /dev/null, etc.
    ENDPOINT_KBD = 0x1,
    ENDPOINT_FB = 0x2,

    ENDPOINT_TERM = 0x3, // Both real and pseudo; only terminal slaves get endpoints

    ENDPOINT_DRIVE = 0x4,
} endpoint_driver_type;

typedef enum {
    ENDPOINT_MEM_GENERIC = 0x0,
    ENDPOINT_MEM_DEVFS = 0x1,
} endpoint_mem_minor;