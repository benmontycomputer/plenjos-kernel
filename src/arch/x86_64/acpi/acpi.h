#ifndef _KERNEL_ACPI_H
#define _KERNEL_ACPI_H

#include <stdint.h>

// https://wiki.osdev.org/RSDT

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
} __attribute__((packed)) RSDP_t;

typedef struct {
    RSDP_t first;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t checksum;
} __attribute__((packed)) RSDP_EXTENDED_t;

typedef struct {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__((packed)) ACPISDTHeader_t;

typedef struct {
    ACPISDTHeader_t h;
    uint32_t PointerToOtherSDT[];//(h.Length - sizeof(h)) / 4];
} __attribute__((packed)) RSDT_t;

typedef struct {
    ACPISDTHeader_t h;
    uint64_t PointerToOtherSDT[];//(h.Length - sizeof(h)) / 8];
} __attribute__((packed)) XSDT_t;

typedef struct {
    ACPISDTHeader_t      common;
    uint32_t             lapic_address;
    uint32_t             flags;
} __attribute__((packed)) acpi_madt_header_t;

typedef struct {
  uint8_t AddressSpace;
  uint8_t BitWidth;
  uint8_t BitOffset;
  uint8_t AccessSize;
  uint64_t Address;
} __attribute__((packed)) GenericAddressStructure;

typedef struct {
    ACPISDTHeader_t h;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t  Reserved;

    uint8_t  PreferredPowerManagementProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BIOS_REQ;
    uint8_t  PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t  PM1EventLength;
    uint8_t  PM1ControlLength;
    uint8_t  PM2ControlLength;
    uint8_t  PMTimerLength;
    uint8_t  GPE0Length;
    uint8_t  GPE1Length;
    uint8_t  GPE1Base;
    uint8_t  CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    // If the second bit is set, there is a PS/2 controller
    uint16_t BootArchitectureFlags;

    uint8_t  Reserved2;
    uint32_t Flags;

    // 12 byte structure; see below for details
    GenericAddressStructure ResetReg;

    uint8_t  ResetValue;
    uint8_t  Reserved3[3];
  
    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                X_FirmwareControl;
    uint64_t                X_Dsdt;

    GenericAddressStructure X_PM1aEventBlock;
    GenericAddressStructure X_PM1bEventBlock;
    GenericAddressStructure X_PM1aControlBlock;
    GenericAddressStructure X_PM1bControlBlock;
    GenericAddressStructure X_PM2ControlBlock;
    GenericAddressStructure X_PMTimerBlock;
    GenericAddressStructure X_GPE0Block;
    GenericAddressStructure X_GPE1Block;
} acpi_fadt_header_t;

typedef struct {

} acpi_dsdt_header_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} acpi_madt_record_t;

typedef struct {
    acpi_madt_record_t  common;
    uint8_t             processor_id;
    uint8_t             apic_id;
    uint32_t            flags;
} __attribute__((packed)) acpi_madt_record_lapic_t;

typedef struct {
    acpi_madt_record_t  common;
    uint8_t             ioapic_id;
    uint8_t             rsv0;
    uint32_t            ioapic_address;
    uint32_t            gsi_base;
} __attribute__((packed)) acpi_madt_record_ioapic_t;

typedef struct {
    acpi_madt_record_t  common;
    uint8_t             bus_source;
    uint8_t             irq_source;
    uint32_t            gsi;
    uint16_t            flags;
} __attribute__((packed)) acpi_madt_record_iso_t;

typedef struct {
    acpi_madt_record_t  common;
    uint8_t             processor_id;
    uint16_t            flags;
    uint8_t             lint_no;
} __attribute__((packed)) acpi_madt_record_nmi_t;

void acpi_init();
void *find_acpi_table(const char *sig);

extern XSDT_t *xsdt_global;
extern acpi_madt_header_t *madt_header_global;
extern acpi_fadt_header_t *fadt_header_global;

#endif