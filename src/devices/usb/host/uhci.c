#include "uhci.h"

#ifdef __KERNEL_SUPPORT_DEV_USB_HOST_UHCI

# include "arch/x86_64/common.h"
# include "devices/io/ports.h"
# include "kernel.h"
# include "lib/stdio.h"
# include "memory/kmalloc.h"
# include "memory/mm.h"
# include "timer/pit.h"
# include "timer/timer.h"

# define UHCI_TD_HEADER(pid, dev_addr, ep_num, data_toggle, max_len)                                           \
     (((pid) & 0xFF) | (((dev_addr) & 0x7F) << 8) | (((ep_num) & 0x0F) << 15) | (((data_toggle) & 0x01) << 19) \
      | (((max_len - 1) & 0x7FF) << 21))

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

static void uhci_free_td_chain(struct uhci_td *td_start) {
    struct uhci_td *td_curr = td_start;

    while (td_curr) {
        struct uhci_td *td_next = NULL;
        if (td_curr->next_td.terminate == 0) {
            uint32_t next_td_phys = td_curr->next_td.ptr << 4;
            td_next               = (struct uhci_td *)phys_to_virt((physptr_t)next_td_phys);
        }

        uhci_free_td(td_curr);
        td_curr = td_next;
    }
}

static inline int link_td(struct uhci_td *prev, struct uhci_td *next) {
    uint32_t phys = get_physaddr32((uint64_t)next, kernel_pml4);

    if (phys == 0) {
        return -1;
    }

    prev->next_td.ptr         = phys >> 4;
    prev->next_td.terminate   = 0;
    prev->next_td.depth_first = 0;
    prev->next_td.type        = 0;
    prev->data_next           = (uint64_t)next;

    return 0;
}

int uhci_insert_qh(uhci_controller_t *controller, struct uhci_qh *qh) {
    /* Don't need to lock QH from the UHCI controller; only protect from other kernel processes */
    int ints = are_interrupts_enabled();
    if (ints) {
        asm volatile("cli");
    }
    mutex_lock(&controller->qh_lock);
    printf("uhci packet: %p, td %p\n\n", get_physaddr32((uint64_t)qh, kernel_pml4),
           get_physaddr(qh->data_first_td, kernel_pml4));

    qh->horiz_link_ptr = controller->qh_1ms.horiz_link_ptr;
    if (!controller->qh_1ms.horiz_link_ptr.enable)
        qh->data_prev = phys_to_virt((uint64_t)controller->qh_1ms.horiz_link_ptr.ptr << 4);

    controller->qh_1ms.horiz_link_ptr
        = (struct uhci_frame_list_entry) { .enable   = 0,
                                           .ptr      = get_physaddr32((uint64_t)qh, kernel_pml4) >> 4,
                                           .type     = UHCI_FRAME_LIST_ENTRY_TYPE_QH,
                                           .reserved = 0 };

    mutex_unlock(&controller->qh_lock);
    if (ints) {
        asm volatile("sti");
    }

    return 0;
}

# define UHCI_WAIT_PRIO 1000

int uhci_wait_td(volatile struct uhci_td *td, uint32_t timeout_ms) {
    uint32_t elapsed = 0;

    /* TODO: use IRQs instead of polling */
    while (td->status & UHCI_TD_ACTIVE) {
        if (elapsed >= timeout_ms) {
            printf("uhci_wait_td: timeout reached\n");
            return -1;
        }

        ksleep_nonblocking_ms(1, UHCI_WAIT_PRIO);
        elapsed++;
    }

    if (td->status & UHCI_TD_ERR_MASK) {
        printf("uhci_wait_td: TD err, status = %.8x %.8x\n", td->status, td->status & UHCI_TD_ERR_MASK);
        return -2;
    }

    return 0;
}

/* All TDs in the chain *should* be done before this function. If not, the driver shouldn't break, but uhci_wait_td()
 * might cause this function to take a decent amount of time as it has to wait for the timeout for each incomplete TD.
 * This function does NOT free the QH or TDs.
 */
int uhci_remove_qh(uhci_controller_t *controller, struct uhci_qh *qh) {
    /* Protect QH from other kernel processes */
    int ints = are_interrupts_enabled();
    if (ints) {
        asm volatile("cli");
    }
    mutex_lock(&controller->qh_lock);

    /* Ensure controller doesn't execute any more TDs in this chain (everything should already be inactive before this
     * function is called, but this is just to be safe) */
    for (struct uhci_td *td = (struct uhci_td *)qh->data_first_td; td; td = (struct uhci_td *)td->data_next) {
        td->next_td.terminate = 1;
        td->next_td.ptr       = 0;
    }

    /* Remove the QH from the chain */
    struct uhci_qh *prev = (struct uhci_qh *)qh->data_prev;
    if (prev) {
        prev->horiz_link_ptr = qh->horiz_link_ptr;
    }

    /* Wait for all TDs to complete */
    for (struct uhci_td *td = (struct uhci_td *)qh->data_first_td; td; td = (struct uhci_td *)td->data_next) {
        uhci_wait_td(td, 100);
    }

    mutex_unlock(&controller->qh_lock);
    if (ints) {
        asm volatile("sti");
    }

    return 0;
}

static int uhci_next_dev_address(uhci_controller_t *controller) {
    int res = -1;

    int ints = are_interrupts_enabled();
    if (ints) {
        asm volatile("cli");
    }
    mutex_lock(&controller->dev_addresses_lock);

    for (int i = 0; i < 127; i++) {
        if (!controller->dev_addresses[i]) {
            controller->dev_addresses[i] = 1;
            res                          = i + 1;
            break;
        }
    }

    if (res < 0) {
        printf("ERROR: uhci_next_dev_address: couldn't allocate device address.\n");
    }

    mutex_unlock(&controller->dev_addresses_lock);
    if (ints) {
        asm volatile("sti");
    }

    return res;
}

static void uhci_free_dev_address(uhci_controller_t *controller, uint8_t addr) {
    if (addr > 127 || addr < 1) {
        printf("ERROR: uhci_free_dev_address: invalid device address %d.\n", addr);
        return;
    }

    int ints = are_interrupts_enabled();
    if (ints) {
        asm volatile("cli");
    }
    mutex_lock(&controller->dev_addresses_lock);

    if (!controller->dev_addresses[addr - 1]) {
        printf("WARN: uhci_free_dev_address: device address %d was not taken.\n", addr);
    }
    controller->dev_addresses[addr - 1] = 1;

    mutex_unlock(&controller->dev_addresses_lock);
    if (ints) {
        asm volatile("sti");
    }
}

int uhci_control_transfer(uhci_controller_t *controller, uint8_t device_address, uint8_t ep0_max_packet,
                          uint8_t *setup_data /* 8 bytes; physaddr must be sub-4G */, uint8_t *data_buffer,
                          uint16_t data_length, bool data_in) {
    int res = 0;

    /* Setup packet */
    struct uhci_qh *qh = uhci_alloc_qh();
    if (!qh) {
        printf("uhci: ERROR: failed to allocate QH for control transfer setup stage.\n");
        return -1;
    }

    struct uhci_td *setup_td = uhci_alloc_td();
    if (!setup_td) {
        printf("uhci: ERROR: failed to allocate TD for control transfer setup stage.\n");
        uhci_free_qh(qh);
        return -1;
    }

    qh->data_first_td = (uint64_t)setup_td;

    setup_td->next_td.terminate = 1;
    setup_td->packet_header     = UHCI_TD_HEADER(UHCI_TD_PID_SETUP, device_address, 0, 0, 8);
    setup_td->status            = UHCI_TD_ACTIVE;
    setup_td->buffer_pointer    = get_physaddr32((physptr_t)setup_data, kernel_pml4);
    if (setup_td->buffer_pointer == 0) {
        printf("uhci: ERROR: setup_data physaddr is NULL or above 4GB limit for UHCI.\n");
        uhci_free_td(setup_td);
        uhci_free_qh(qh);
        return -1;
    }

    qh->horiz_link_ptr.enable = 1;

    qh->element_link_ptr.enable = 0;
    qh->element_link_ptr.type   = UHCI_FRAME_LIST_ENTRY_TYPE_TD;
    qh->element_link_ptr.ptr    = (get_physaddr32((uint64_t)setup_td, kernel_pml4)) >> 4;

    /* Data */
    struct uhci_td *prev = setup_td;
    if (data_length > 0) {
        if (!data_buffer) {
            printf("ERROR: uhci_control_transfer: data_length > 0 but data_buffer is NULL. Not transferring.\n");
            res = -EINVAL;
            goto cleanup;
        }

        size_t off = 0;
        int toggle = 1;

        while (off < data_length) {
            size_t pkt_len = min(data_length - off, ep0_max_packet);

            struct uhci_td *td_next = uhci_alloc_td();
            if (!td_next) {
                printf("ERROR: uhci_control_transfer: couldn't allocate transfer descriptor.\n");
                res = -ENOMEM;
                goto cleanup;
            }

            link_td(prev, td_next);

            td_next->next_td.terminate = 1;
            td_next->packet_header
                = UHCI_TD_HEADER(data_in ? UHCI_TD_PID_IN : UHCI_TD_PID_OUT, device_address, 0, toggle, pkt_len);
            td_next->status = UHCI_TD_ACTIVE | UHCI_TD_SPD;

            td_next->buffer_pointer = get_physaddr32((uint64_t)data_buffer + off, kernel_pml4);
            if (td_next->buffer_pointer == 0) {
                printf("ERROR: uhci_control_transfer: buffer pointer is NULL or above-4G.\n");
                res = -EINVAL;
                goto cleanup;
            }

            prev = td_next;

            off    += pkt_len;
            toggle ^= 1;
        }
    }

    /* Status packet (for interrupt) */
    struct uhci_td *status_td = uhci_alloc_td();
    if (!status_td) {
        printf("ERROR: uhci_control_transfer: couldn't allocate transfer descriptor.\n");
        res = -ENOMEM;
        goto cleanup;
    }

    link_td(prev, status_td);

    status_td->next_td.terminate = 1;
    status_td->packet_header     = UHCI_TD_HEADER(data_in ? UHCI_TD_PID_OUT : UHCI_TD_PID_IN, device_address, 0, 1, 0);
    status_td->status            = UHCI_TD_ACTIVE | UHCI_TD_IOC;
    status_td->buffer_pointer    = 0U;

    /* Add into QH schedule */
    uhci_insert_qh(controller, qh);

    res = uhci_wait_td(status_td, UHCI_DEFAULT_CONTROL_TIMEOUT);

    if (res == -1) {
        /* Timeout */
        printf("ERROR: uhci_control_transfer: timeout reached\n");
        printf("cmdreg: %.4x, stsreg: %.4x\n", uhci_read_register(controller, UHCI_IO_USBCMD),
               uhci_read_register(controller, UHCI_IO_USBSTS));
    } else if (res == -2) {
        printf("ERROR: uhci_control_transfer: TD error mask set\n");
        res = -1;
    }

cleanup:
    uhci_free_td_chain(setup_td);
    uhci_free_qh(qh);

    return res;
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

static bool uhci_host_driver_populated         = false;
static struct usb_host_driver uhci_host_driver = (struct usb_host_driver) { 0 };

static int uhci_host_driver_control_transfer_func(usb_device_t *dev, uint8_t *setup_packet, uint8_t *data_buffer,
                                                  uint16_t data_length, bool data_in) {
    return uhci_control_transfer((uhci_host_driver_data_t *)dev->host_driver_data, dev->address,
                                 data_in ? dev->ep0_in.max_packet_size : dev->ep0_out.max_packet_size, setup_packet,
                                 data_buffer, data_length, data_in);
}

void uhci_device_connected_callback_func(void *data) {
    struct uhci_dev_status_poll_args *args = (struct uhci_dev_status_poll_args *)data;
    const uint16_t reg_offset              = UHCI_IO_PORTSC1 + (args->port_num * 2);

    uint16_t port_status = uhci_read_register(args->controller, reg_offset);
    if (!(port_status & UHCI_PORTSCREG_CONN_STATUS)) {
        printf("uhci: no device connected on port %d.\n", args->port_num);
        return;
    }

    printf("uhci: device connected on port %d.\n", args->port_num);

    /* Clear the CSC bit (by writing 1 to it) */
    uhci_set_register_bits(args->controller, reg_offset, UHCI_PORTSCREG_CONN_STATUS_CHANGED);
    ksleep_nonblocking_ms(100, UHCI_WAIT_PRIO);

    /* Verify connection status is still set */
    port_status = uhci_read_register(args->controller, UHCI_IO_PORTSC1 + (args->port_num * 2));
    if (!(port_status & UHCI_PORTSCREG_CONN_STATUS)) {
        printf("uhci: error: device no longer connected on port %d.\n", args->port_num);
        return;
    }

    /* Reset the port */
    uhci_set_register_bits(args->controller, reg_offset, UHCI_PORTSCREG_RESET);
    ksleep_nonblocking_ms(100, UHCI_WAIT_PRIO);
    uhci_clear_register_bits(args->controller, reg_offset, UHCI_PORTSCREG_RESET);
    ksleep_nonblocking_ms(50, UHCI_WAIT_PRIO);
    uhci_set_register_bits(args->controller, reg_offset, UHCI_PORTSCREG_PORT_ENABLE);
    port_status = uhci_read_register(args->controller, UHCI_IO_PORTSC1 + (args->port_num * 2));

    uint16_t cnt = 0;

    /* Wait for device enable (100ms max) */
    while (!(port_status & UHCI_PORTSCREG_PORT_ENABLE) && (cnt < 100)) {
        port_status = uhci_read_register(args->controller, UHCI_IO_PORTSC1 + (args->port_num * 2));
        ksleep_nonblocking_ms(10, UHCI_WAIT_PRIO);
        cnt += 10;
    }

    if (!(port_status & UHCI_PORTSCREG_PORT_ENABLE) && (cnt < 100)) {
        printf("uhci: error: device reset failed on port %d\n", args->port_num);
        return;
    }

    printf("uhci: device on port %d has status %.4x\n", args->port_num, port_status);

    /* Set address */
    int addr_res = uhci_next_dev_address(args->controller);
    if (addr_res <= 0 || addr_res > 127) {
        /* TODO: print controller details for easier debugging */
        printf("ERROR: uhci_device_connected_callback_func: no more free device addresses.\n");
        return;
    }

    if (!uhci_host_driver_populated) {
        uhci_host_driver           = usb_populate_host_driver(uhci_host_driver_control_transfer_func);
        uhci_host_driver_populated = true;
    }

    usb_device_t *dev = usb_setup_device(NULL, &args->controller->bus, (uint8_t)addr_res, args->port_num,
                                         &uhci_host_driver, (uhci_host_driver_data_t *)args->controller);

    if (!dev) {
        printf("ERROR: uhci_device_connected_callback_func: usb_setup_device() failed.\n");
        uhci_free_dev_address(args->controller, addr_res);
    }
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
    ksleep_nonblocking_ms(10, UHCI_WAIT_PRIO);
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
    controller->frame_list_phys = find_next_free_frame();

    if (controller->frame_list_phys == 0 || controller->frame_list_phys >= UINT32_MAX) {
        printf("ERROR: uhci_init failed to allocate frame list memory!\n");
        return -1;
    }

    controller->frame_list_base = (struct uhci_frame_list_entry *)phys_to_virt(controller->frame_list_phys);
    memset(controller->frame_list_base, 0, 4096);

    /* 32 -> 16 -> 8 -> 4 -> 2 -> 1 ms queue heads */
    controller->qh_1ms.horiz_link_ptr.ptr  = 0;
    controller->qh_2ms.horiz_link_ptr.ptr  = get_physaddr32((uint64_t)&controller->qh_1ms, kernel_pml4) >> 4;
    controller->qh_4ms.horiz_link_ptr.ptr  = get_physaddr32((uint64_t)&controller->qh_2ms, kernel_pml4) >> 4;
    controller->qh_8ms.horiz_link_ptr.ptr  = get_physaddr32((uint64_t)&controller->qh_4ms, kernel_pml4) >> 4;
    controller->qh_16ms.horiz_link_ptr.ptr = get_physaddr32((uint64_t)&controller->qh_8ms, kernel_pml4) >> 4;
    controller->qh_32ms.horiz_link_ptr.ptr = get_physaddr32((uint64_t)&controller->qh_16ms, kernel_pml4) >> 4;

    controller->qh_1ms.horiz_link_ptr.type  = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
    controller->qh_2ms.horiz_link_ptr.type  = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
    controller->qh_4ms.horiz_link_ptr.type  = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
    controller->qh_8ms.horiz_link_ptr.type  = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
    controller->qh_16ms.horiz_link_ptr.type = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
    controller->qh_32ms.horiz_link_ptr.type = UHCI_FRAME_LIST_ENTRY_TYPE_QH;

    /* 1ms is end of link */
    controller->qh_1ms.horiz_link_ptr.enable = 1;

    /* Disable vertical pointers */
    controller->qh_1ms.element_link_ptr.enable  = 1;
    controller->qh_2ms.element_link_ptr.enable  = 1;
    controller->qh_4ms.element_link_ptr.enable  = 1;
    controller->qh_8ms.element_link_ptr.enable  = 1;
    controller->qh_16ms.element_link_ptr.enable = 1;
    controller->qh_32ms.element_link_ptr.enable = 1;

    for (int i = 0; i < 1024; i++) {
        if ((i % 32) == 0) {
            controller->frame_list_base[i].enable = 0;
            controller->frame_list_base[i].type   = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
            controller->frame_list_base[i].ptr    = get_physaddr32((uint64_t)&controller->qh_32ms, kernel_pml4) >> 4;
        } else if ((i % 16 == 0)) {
            controller->frame_list_base[i].enable = 0;
            controller->frame_list_base[i].type   = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
            controller->frame_list_base[i].ptr    = get_physaddr32((uint64_t)&controller->qh_16ms, kernel_pml4) >> 4;
        } else if ((i % 8 == 0)) {
            controller->frame_list_base[i].enable = 0;
            controller->frame_list_base[i].type   = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
            controller->frame_list_base[i].ptr    = get_physaddr32((uint64_t)&controller->qh_8ms, kernel_pml4) >> 4;
        } else if ((i % 4 == 0)) {
            controller->frame_list_base[i].enable = 0;
            controller->frame_list_base[i].type   = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
            controller->frame_list_base[i].ptr    = get_physaddr32((uint64_t)&controller->qh_4ms, kernel_pml4) >> 4;
        } else if ((i % 2 == 0)) {
            controller->frame_list_base[i].enable = 0;
            controller->frame_list_base[i].type   = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
            controller->frame_list_base[i].ptr    = get_physaddr32((uint64_t)&controller->qh_2ms, kernel_pml4) >> 4;
        } else {
            controller->frame_list_base[i].enable = 0;
            controller->frame_list_base[i].type   = UHCI_FRAME_LIST_ENTRY_TYPE_QH;
            controller->frame_list_base[i].ptr    = get_physaddr32((uint64_t)&controller->qh_1ms, kernel_pml4) >> 4;
        }
    }

    uhci_write_register(controller, UHCI_IO_FRBASEADD, (uint16_t)(controller->frame_list_phys & 0xFFFF));
    uhci_write_register(controller, UHCI_IO_FRBASEADD + 2, (uint16_t)((controller->frame_list_phys >> 16) & 0xFFFF));

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

    /* Setup controller bus */
    /* TODO: do this */
    controller->bus.id   = 0;
    controller->bus.data = controller;

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

/* TODO: consider using mass-allocated blocks instead of individual kmalloc_heap calls */
void *uhci_alloc_td() {
    void *td = kmalloc_heap(sizeof(struct uhci_td));
    if (td && !get_physaddr32((uint64_t)td, kernel_pml4)) {
        printf("ERROR: uhci: couldn't allocate td below 4G.\n");
        kfree_heap(td); /* Don't need to check for td == NULL because we know td >> 32 is non-zero */
        return NULL;
    }
    if (td) {
        memset(td, 0, sizeof(struct uhci_td));
    }
    return td;
}

void *uhci_alloc_qh() {
    void *qh = kmalloc_heap(sizeof(struct uhci_qh));
    if (qh && !get_physaddr32((uint64_t)qh, kernel_pml4)) {
        printf("ERROR: uhci: couldn't allocate qh below 4G.\n");
        kfree_heap(qh); /* Don't need to check for qh == NULL because we know qh >> 32 is non-zero */
        return NULL;
    }
    if (qh) {
        memset(qh, 0, sizeof(struct uhci_qh));
    }
    return qh;
}

void uhci_free_td(void *td) {
    kfree_heap(td);
}

void uhci_free_qh(void *qh) {
    kfree_heap(qh);
}

#endif // __KERNEL_SUPPORT_DEV_USB_UHCI