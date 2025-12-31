#include "uhci.h"

#ifdef __KERNEL_SUPPORT_DEV_USB_HOST_UHCI

# include "devices/io/ports.h"
# include "kernel.h"
# include "lib/stdio.h"
# include "memory/kmalloc.h"
# include "memory/mm.h"
# include "timer/pit.h"
# include "timer/timer.h"

static inline void uhci_write_register(uhci_controller_t *controller, uint16_t reg_offset, uint16_t value) {
    outw(controller->io_base + reg_offset, value);
}

static inline uint16_t uhci_read_register(uhci_controller_t *controller, uint16_t reg_offset) {
    return inw(controller->io_base + reg_offset);
}

static inline void uhci_set_register_bits(uhci_controller_t *controller, uint16_t reg_offset, uint16_t bits) {
    uint16_t reg  = uhci_read_register(controller, reg_offset);
    reg          |= bits;
    uhci_write_register(controller, reg_offset, reg);
}

static inline void uhci_clear_register_bits(uhci_controller_t *controller, uint16_t reg_offset, uint16_t bits) {
    uint16_t reg  = uhci_read_register(controller, reg_offset);
    reg          &= ~bits;
    uhci_write_register(controller, reg_offset, reg);
}

struct uhci_dev_status_poll_args {
    uhci_controller_t *controller;
    uint8_t port_num;
    uint16_t desired_status_set_mask;
    uint16_t desired_status_clear_mask;
    uint16_t timeout_ms;
    uint8_t repeat_count;

    bool free_after_completion;

    void (*callback)(void *data);
    void *callback_data;
};

void uhci_dev_status_poll_callback(struct timer_timeout *data) {
    struct uhci_dev_status_poll_args *args = (struct uhci_dev_status_poll_args *)data->data;

    uint16_t port_status = uhci_read_register(args->controller, UHCI_IO_PORTSC1 + (args->port_num * 2));

    if ((port_status & args->desired_status_set_mask) == args->desired_status_set_mask
        && (port_status & args->desired_status_clear_mask) == 0) {
        // Desired status reached

        if (args->callback) {
            args->callback(args->callback_data);
        }

        // We free this afterwords since the callback might want to access it
        if (args->free_after_completion) kfree_heap(args);
        return;
    }

    if (args->repeat_count > 0) {
        args->repeat_count--;
        set_timeout(args->timeout_ms, uhci_dev_status_poll_callback, data);
    } else {
        printf("uhci: port %d did not reach desired status of 0x%04x set and 0x%04x clear within timeout\n",
               args->port_num, args->desired_status_set_mask, args->desired_status_clear_mask);
        if (args->free_after_completion) kfree_heap(args);
    }
}

void uhci_device_connected_callback_func(void *data) {
    struct uhci_dev_status_poll_args *args = (struct uhci_dev_status_poll_args *)data;

    printf("uhci: device connected on port %d\n", args->port_num);
}

int uhci_init(uhci_controller_t *controller, pci_device_t *pci_dev) {
    /* First, make sure our pci_dev is actually an UHCI controller */
    if (pci_dev->class_code != PCI_CLASS_SERIAL_BUS) {
        printf(
            "ERROR: uhci_init was passed a pci device that isn't a serial bus controller (expected class %.2x, actual "
            "%.2x)!\n",
            PCI_CLASS_SERIAL_BUS, pci_dev->class_code);
        return -1;
    }

    if (pci_dev->subclass_code != PCI_SUBCLASS_SERIAL_BUS_CONTROLLER_USB) {
        printf("ERROR: uhci_init was passed a serial bus controller that isn't a USB controller (expected subclass "
               "%.2x, actual %.2x)!\n",
               PCI_SUBCLASS_SERIAL_BUS_CONTROLLER_USB, pci_dev->subclass_code);
        return -1;
    }

    if (pci_dev->prog_if != PCI_PROGIF_SERIAL_BUS_CONTROLLER_USB_CONTROLLER_UHCI) {
        printf("ERROR: uhci_init was passed a usb controller that isn't an UHCI host controller (expected progif %.2x, "
               "actual %.2x)!\n",
               PCI_PROGIF_SERIAL_BUS_CONTROLLER_USB_CONTROLLER_UHCI, pci_dev->prog_if);
        return -1;
    }

    /* Next, start populating the controller struct */
    memset(controller, 0, sizeof(uhci_controller_t));
    controller->pci_dev = pci_dev;

    /* The I/O base address can technically be at any BAR; we must find one with bit 0 set */
    for (int bar_idx = 0; bar_idx < 6; bar_idx++) {
        uint32_t bar_value = 0;
        switch (bar_idx) {
        case 0:
            bar_value = pci_dev->bar0;
            break;
        case 1:
            bar_value = pci_dev->bar1;
            break;
        case 2:
            bar_value = pci_dev->bar2;
            break;
        case 3:
            bar_value = pci_dev->bar3;
            break;
        case 4:
            bar_value = pci_dev->bar4;
            break;
        case 5:
            bar_value = pci_dev->bar5;
            break;
        }

        if (bar_value & 0x1) {
            controller->io_base = bar_value & ~0x3; /* Mask off the bottom 2 bits */
            break;
        }
    }

    /**
     * Steps to initialize UHCI controller:
     *  1. Disable BIOS USB legacy support (if applicable)
     *  2. Set I/O bus mastering in PCI
     *  3. Reset the controller
     *  4. Setup frame list
     *  5. Enable interrupts
     *  6. Find out number of ports
     *  7. Start the controller
     */

    /**
     * Step 1: Disable BIOS USB legacy support by writing 0x2000 to the PCI register 0xC0
     */
    pci_write(pci_dev->bus, pci_dev->device, pci_dev->function, 0xC0, 0x2000);

    /**
     * Step 2: Set I/O bus mastering in PCI command register, as well as enable I/O space
     */
    pci_set_command_bits(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CMD_IO_SPACE | PCI_CMD_BUS_MASTER);

    /**
     * Step 3: Reset the controller by stopping it, setting UHCI_CMDREG_GLOBAL_RESET (will need to be manually cleared
     * after 10ms), and setting UHCI_CMDREG_HC_RESET and waiting for it to clear.
     */
    uhci_clear_register_bits(controller, UHCI_IO_USBCMD, UHCI_CMDREG_RUN);

    uhci_set_register_bits(controller, UHCI_IO_USBCMD, UHCI_CMDREG_GLOBAL_RESET);
    pit_sleep(10);
    uhci_clear_register_bits(controller, UHCI_IO_USBCMD, UHCI_CMDREG_GLOBAL_RESET);

    uhci_set_register_bits(controller, UHCI_IO_USBCMD, UHCI_CMDREG_HC_RESET);

    while (uhci_read_register(controller, UHCI_IO_USBCMD) & UHCI_CMDREG_HC_RESET) {
        // Wait for HC_RESET to clear
    }

    /**
     * Step 4: Setup frame list:
     *  a. Allocate 4k-aligned physical memory for frame list (1024 entries of 4 bytes each)
     *  b. Initialize all entries to disabled (bit 0 = 1)
     *  c. Write physical address of frame list to FRBASEADD register
     */
    physptr_t frame_list_phys = find_next_free_frame();

    if (frame_list_phys == 0 || frame_list_phys >= UINT32_MAX) {
        printf("ERROR: uhci_init failed to allocate frame list memory!\n");
        return -1;
    }

    struct uhci_frame_list_entry *frame_list_virt = (struct uhci_frame_list_entry *)phys_to_virt(frame_list_phys);
    memset(frame_list_virt, 0, 4096);

    for (int i = 0; i < 1024; i++) {
        frame_list_virt[i].enable = 1;
    }

    uhci_write_register(controller, UHCI_IO_FRBASEADD, (uint16_t)(frame_list_phys & 0xFFFF));
    uhci_write_register(controller, UHCI_IO_FRBASEADD + 2, (uint16_t)((frame_list_phys >> 16) & 0xFFFF));

    /**
     * Step 5: Enable interrupts by setting bits in USBINTR register
     */
    uhci_set_register_bits(controller, UHCI_IO_USBINTR,
                           UHCI_INTRREG_TIMEOUTCRC | UHCI_INTRREG_RESUME | UHCI_INTRREG_COMPLETE
                               | UHCI_INTRREG_SHORTPACKET);

    /**
     * Step 6: Find out number of ports. This isn't stored anywhere, so we have to check all port registers. If bit
     * 7 is set and the value is not 0xFFFF, then we have a valid port.
     */
    int num_ports = 0;
    for (int port_idx = 0; port_idx < 8; port_idx++) {
        uint16_t portsc = uhci_read_register(controller, UHCI_IO_PORTSC1 + (port_idx * 2));
        if ((portsc & 0x80) && (portsc != 0xFFFF)) {
            num_ports++;
        } else {
            printf("uhci: end port register = %d\n", portsc);
            break;
        }
    }

    printf("uhci: detected %d ports on UHCI controller at PCI %02x:%02x.%x (BAR 0x%p)\n", num_ports, pci_dev->bus,
           pci_dev->device, pci_dev->function, controller->io_base);

    /**
     * Step 7: Start the controller by setting UHCI_CMDREG_RUN and UHCI_CMDREG_MAX_PACKET_SIZE (to enable 64-byte
     * transfers) in USBCMD register
     */
    uhci_set_register_bits(controller, UHCI_IO_USBCMD, UHCI_CMDREG_RUN | UHCI_CMDREG_MAX_PACKET_SIZE);

    struct uhci_dev_status_poll_args *data
        = (struct uhci_dev_status_poll_args *)kmalloc_heap(sizeof(struct uhci_dev_status_poll_args));
    if (!data) {
        printf("ERROR: uhci_init failed to allocate memory for device status poll args!\n");
        return -1;
    }

    data->controller                = controller;
    data->desired_status_set_mask   = UHCI_PORTSCREG_CONN_STATUS_CHANGED;
    data->desired_status_clear_mask = 0;
    data->timeout_ms                = 100;
    data->port_num                  = 0;  // TODO: support multiple ports
    data->repeat_count              = 50; // Try for 5 seconds

    data->free_after_completion = true;

    data->callback      = uhci_device_connected_callback_func;
    data->callback_data = data;

    set_timeout(100, uhci_dev_status_poll_callback, data);
    return 0;
}

#endif // __KERNEL_SUPPORT_DEV_USB_UHCI