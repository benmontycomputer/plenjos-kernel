#include <stdint.h>

#include "devices/storage/ide.h"
#include "devices/pci/pci.h"

#include "lib/stdio.h"

void ide_init() {
    // Scanning PCI bus for IDE controllers
    for (uint32_t i = 0; i < pci_mass_storage_controller_count; i++) {
        pci_device_t dev = pci_mass_storage_controllers[i];

        if (dev.subclass_code == PCI_SUBCLASS_IDE) {
            // Initialize IDE controller
            printf("Found IDE Controller: %s %s (Bus %d, Device %d, Function %d)\n",
                   get_vendor_pretty(dev.vendor_id),
                   /* dev.device_name */ "{unknown}",
                   dev.bus,
                   dev.device,
                   dev.function);

            printf("  Mode: %s\n", (dev.prog_if & PCI_PROGIF_IDE_MASTER_DMA) ? "Master/DMA" : "No DMA");
            printf("  BAR0: %x\n", dev.bar0);
            printf("  BAR1: %x\n", dev.bar1);
            printf("  BAR2: %x\n", dev.bar2);
            printf("  BAR3: %x\n", dev.bar3);
            printf("  BAR4: %x\n", dev.bar4);
            printf("  BAR5: %x\n", dev.bar5);
            printf("  Primary channel status: %s mode, %s\n", (dev.prog_if & PCI_PROGIF_IDE_C0_PCI_NATIVE_MODE) ? "PCI Native" : "Compatibility", (dev.prog_if & PCI_PROGIF_IDE_C0_SWITCHABLE) ? "Switchable" : "Not Switchable");
            printf("  Secondary channel status: %s mode, %s\n", (dev.prog_if & PCI_PROGIF_IDE_C1_PCI_NATIVE_MODE) ? "PCI Native" : "Compatibility", (dev.prog_if & PCI_PROGIF_IDE_C1_SWITCHABLE) ? "Switchable" : "Not Switchable");
        }
    }
}