#include "usbhid.h"

#ifdef __KERNEL_SUPPORT_DEV_USB_DEVICE_HID

# include "devices/input/keyboard/keyboard.h"
# include "lib/stdio.h"
# include "timer/timer.h"

static const uint8_t usbkbd_getreport_packet[8] = { 0xA1, 0x01, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00 };

static const enum key_codes usbkbd_boot_modcodes[8]
    = { KEY_LEFT_CTRL,  KEY_LEFT_SHIFT,  KEY_LEFT_ALT,  KEY_LEFT_META,
        KEY_RIGHT_CTRL, KEY_RIGHT_SHIFT, KEY_RIGHT_ALT, KEY_RIGHT_META };

static inline uint8_t usbkbd_boot_packet_to_event_mods(uint8_t packet_mods) {
    uint8_t res = 0;

    /* TODO: implement caps lock */

    if (packet_mods & 0x01 || packet_mods & 0x10) res |= KBD_MOD_CTRL;
    if (packet_mods & 0x02 || packet_mods & 0x20) res |= KBD_MOD_SHIFT;
    if (packet_mods & 0x04 || packet_mods & 0x40) res |= KBD_MOD_ALT;
    if (packet_mods & 0x08 || packet_mods & 0x80) res |= KBD_MOD_META;

    return res;
}

static const enum key_codes usbkbd_boot_specials[]
    = { KEY_ENTER,   KEY_ESC,      KEY_BACKSPACE,    KEY_TAB,         KEY_SPACE,     KEY_MINUS,   KEY_EQUALS,
        KEY_LBRACK,  KEY_RBRACK,   KEY_BACKSLASH,    KEY_UNKNOWN,     KEY_SEMICOLON, KEY_QUOTE,   KEY_BACKTICK,
        KEY_COMMA,   KEY_PERIOD,   KEY_SLASH,        KEY_CAPS_LOCK,   KEY_F1,        KEY_F2,      KEY_F3,
        KEY_F4,      KEY_F5,       KEY_F6,           KEY_F7,          KEY_F8,        KEY_F9,      KEY_F10,
        KEY_F11,     KEY_F12,      KEY_PRINT_SCREEN, KEY_SCROLL_LOCK, KEY_PAUSE,     KEY_INSERT,  KEY_HOME,
        KEY_PAGE_UP, KEY_DELETE,   KEY_END,          KEY_PAGE_DOWN,   KEY_RIGHT,     KEY_LEFT,    KEY_DOWN,
        KEY_UP,      KEY_NUM_LOCK, KEY_KP_SLASH,     KEY_KP_ASTERISK, KEY_KP_MINUS,  KEY_KP_PLUS, KEY_KP_ENTER,
        KEY_KP_1,    KEY_KP_2,     KEY_KP_3,         KEY_KP_4,        KEY_KP_5,      KEY_KP_6,    KEY_KP_7,
        KEY_KP_8,    KEY_KP_9,     KEY_KP_0,         KEY_KP_PERIOD };

static inline void usbkbd_boot_send_from_packet(uint8_t packet, bool press, uint8_t mods_translated) {
    kbd_event_t event = { 0 };
    event.state       = press ? KEY_PRESSED : KEY_RELEASED;
    event.mods        = mods_translated;

    enum key_codes res_code;

    if (packet >= 0x04 && packet <= 0x1D) {
        /* A-Z */
        res_code = KEY_A + packet - 0x04;
    } else if (packet >= 0x1E && packet <= 0x27) {
        /* Numbers 1-0 */
        res_code = KEY_1 + packet - 0x1E;
    } else if (packet >= 0x28 && packet <= 0x63) {
        res_code = usbkbd_boot_specials[packet - 0x28];
    } else {
        return;
    }

    event.code = res_code;
    kbd_buffer_push(event);
}

void usbkbd_poll_timeout_func(struct timer_timeout *timeout) {
    usb_interface_t *iface = (usb_interface_t *)timeout->data;
    struct usbhid_interface_driver_data *iface_driver_data
        = (struct usbhid_interface_driver_data *)iface->iface_driver_data;

    uint8_t buf[8] = { 0 };

    usb_control_transfer(iface->device, usbkbd_getreport_packet, buf, 8, true, 0);

    uint8_t *prev           = iface_driver_data->last_boot_status;
    uint8_t mods            = buf[0];
    uint8_t mods_translated = usbkbd_boot_packet_to_event_mods(mods);

    if (mods == prev[0]) {
        /* No change in modifiers */
    } else {
        /* Bits 7-0: R_SUPER R_ALT R_SHIFT R_CTRL L_SUPER L_ALT L_SHIFT L_CTRL */
        for (int i = 0; i < 8; i++) {
            bool cur_status  = (mods >> i) & 0x1;
            bool prev_status = (prev[0] >> i) & 0x1;

            bool send  = false;
            bool press = false;

            if (cur_status) {
                if (!prev_status) {
                    /* Press */
                    send  = true;
                    press = true;
                }
            } else if (prev_status) {
                /* Release */
                send = true;
            }

            if (!send) continue;

            kbd_event_t event = { 0 };
            event.state       = press ? KEY_PRESSED : KEY_RELEASED;
            event.code        = usbkbd_boot_modcodes[i];
            event.mods        = mods_translated;

            kbd_buffer_push(event);
        }
    }

    /* Check for releases */
    for (uint8_t prev_i = 2; prev_i < 8; prev_i++) {
        uint8_t code  = prev[prev_i];
        bool released = true;
        for (uint8_t buf_i = 2; buf_i < 8; buf_i++) {
            if (buf[buf_i] == code) {
                released = false;
                break;
            }
        }

        if (released) usbkbd_boot_send_from_packet(code, false, mods_translated);
    }

    /* Check for presses */
    for (uint8_t buf_i = 2; buf_i < 8; buf_i++) {
        uint8_t code = buf[buf_i];
        bool pressed = true;
        for (uint8_t prev_i = 2; prev_i < 8; prev_i++) {
            if (prev[prev_i] == code) {
                pressed = false;
                break;
            }
        }

        if (pressed) usbkbd_boot_send_from_packet(code, true, mods_translated);
    }

end:
    memcpy(prev, buf, 8);
    set_timeout(10, usbkbd_poll_timeout_func, iface);
}

int usbkbd_driver_init(usb_interface_t *iface) {
    int res = usbhid_set_protocol(iface, false);

    if (res != 0) {
        printf("ERROR: usbkbd_driver_init: failed to switch to boot protocol!\n");
        return -EIO;
    }

    set_timeout(10, usbkbd_poll_timeout_func, iface);
    return 0;
}

#endif // __KERNEL_SUPPORT_DEV_USB_DEVICE_HID