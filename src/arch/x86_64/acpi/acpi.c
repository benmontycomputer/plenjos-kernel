#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "limine.h"

#include "lib/stdio.h"
#include "lib/string.h"

#include "arch/x86_64/acpi/acpi.h"
#include "arch/x86_64/apic/apic.h"
#include "memory/mm.h"

#include "kernel.h"

// https://wiki.osdev.org/RSDT

// Get RSDP info
__attribute__((used, section(".limine_requests"))) static volatile struct limine_rsdp_request rsdp_request
    = { .id = LIMINE_RSDP_REQUEST, .revision = 0 };

bool checksum_sdt(ACPISDTHeader_t *tableHeader) {
    unsigned char sum = 0;

    for (uint32_t i = 0; i < tableHeader->Length; i++) {
        sum += ((char *)tableHeader)[i];
    }

    return sum == 0;
}

RSDT_t *rsdt_global = NULL;
XSDT_t *xsdt_global = NULL;
acpi_madt_header_t *madt_header_global;
acpi_fadt_header_t *fadt_header_global;
RSDP_t *rsdp = NULL;
RSDP_EXTENDED_t *rsdp_extended = NULL;

bool ps2_detected = false;

int get_entry_count() {
    if (xsdt_global) return (xsdt_global->h.Length - sizeof(xsdt_global->h)) / 8;
    else
        return (rsdt_global->h.Length - sizeof(rsdt_global->h)) / 4;
}

// Returns the virtual address
void *find_acpi_table(const char *sig) {
    int entries = get_entry_count();

    for (int i = 0; i < entries; i++) {
        ACPISDTHeader_t *h;

        if (xsdt_global) {
            h = (ACPISDTHeader_t *)phys_to_virt((uint64_t)xsdt_global->PointerToOtherSDT[i]);
        } else {
            h = (ACPISDTHeader_t *)phys_to_virt((uint64_t)rsdt_global->PointerToOtherSDT[i]);
        }

        if (strncmp(h->Signature, sig, 4) == 0) return (void *)h;
    }

    return NULL;
}

// bool use_xsdt = false;

uint64_t acpi_map_table_mem(uint64_t phys, size_t size) {
    uint64_t offset = phys & 0xFFF;

    map_virtual_memory(phys - offset, (uint64_t)size + offset, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE);

    return phys_to_virt(phys);
}

void acpi_init() {
    if (!rsdp_request.response) {
        printf("Couldn't detect acpi tables. Halt!\n");
        hcf();
    }

    struct limine_rsdp_response *rsdp_resp = rsdp_request.response;

    // if (rsdp->revision == 2) use_xsdt = true;

    rsdp = (RSDP_t *)acpi_map_table_mem((uint64_t)rsdp_resp->address, sizeof(XSDT_t));

    if (rsdp->revision > 0) {
        rsdp_extended = (RSDP_EXTENDED_t *)rsdp_extended;

        xsdt_global = (XSDT_t *)acpi_map_table_mem(rsdp_extended->xsdt_addr, sizeof(XSDT_t));

        if (!checksum_sdt(&(xsdt_global->h))) {
            printf("\nCorrupted xsdt table. Halt!\n");
            hcf();
        }
    } else {
        rsdt_global = (RSDT_t *)acpi_map_table_mem((uint64_t)rsdp->rsdt_addr, sizeof(RSDT_t));

        if (!checksum_sdt(&(rsdt_global->h))) {
            printf("\nCorrupted rsdt table. Halt!\n");
            hcf();
        }
    }

    printf("RSDP revision number: %d\n", rsdp->revision);

    int entries = get_entry_count();

    for (int i = 0; i < entries; i++) {
        ACPISDTHeader_t *h;
        if (xsdt_global) {
            h = (ACPISDTHeader_t *)acpi_map_table_mem((uint64_t)xsdt_global->PointerToOtherSDT[i],
                                                      sizeof(ACPISDTHeader_t));
        } else {
            h = (ACPISDTHeader_t *)acpi_map_table_mem((uint64_t)rsdt_global->PointerToOtherSDT[i],
                                                      sizeof(ACPISDTHeader_t));
        }

        if (!checksum_sdt(h)) {
            printf("Corrupted acpi table. Halt!\n");
            hcf();
        }
    }

    void *apic = find_acpi_table("APIC");

    if (!apic) {
        printf("Couldn't detect MADT table. Halt!\n");
        hcf();
    }

    madt_header_global = (acpi_madt_header_t *)apic;

    // Map the memory for the local apic (each cpu has one, but the address is the same)
    map_virtual_memory(madt_header_global->lapic_address, 4096, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE);
    LAPIC_BASE = phys_to_virt(madt_header_global->lapic_address);

    void *fapc = find_acpi_table("FACP");

    if (!fapc) {
        printf("Couldn't detect FADT table. Halt!\n");
        hcf();
    }

    fadt_header_global = (acpi_fadt_header_t *)fapc;

    printf("\n%b\n\n", fadt_header_global->BootArchitectureFlags);

    if (fadt_header_global->BootArchitectureFlags & 0b10) {
        printf("PS/2 Controller detected!\n");

        ps2_detected = true;
    } else {
        printf("No PS/2 Controller detected.\n");
    }
}