#pragma once

#include "memory/mm.h"

#include <stdbool.h>
#include <stdint.h>

#define VM_DUMP_USER_HALF   0x1
#define VM_DUMP_KERNEL_HALF 0x2
#define VM_DUMP_ALL         (VM_DUMP_USER_HALF | VM_DUMP_KERNEL_HALF)

/**
 * Walks the given PML4 and prints every mapped virtual range, coalescing neighbouring pages that have the same
 * effective permissions and page size. Permissions are the *effective* ones (write/user ANDed and NX ORed across all
 * levels), which is what the CPU actually enforces.
 *
 * Call this after acpi/ioapic init so the LAPIC/IOAPIC annotations are valid.
 */
void vm_dump(const char *label, pml4_t *pml4, uint32_t halves);

/** True if the kernel command line contains the standalone word "vmdump". */
bool vm_dump_requested(const char *cmdline);
