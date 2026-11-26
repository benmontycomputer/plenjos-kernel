#pragma once

#include <stdint.h>
#include "stddef.h"

#include "types.h"

int mmap_fixed(void *addr, size_t length);