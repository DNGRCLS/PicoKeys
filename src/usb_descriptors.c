// USB descriptors: composite device with a boot-protocol keyboard and a
// mouse (wheel + horizontal pan) + consumer-control (media keys) interface.
#include <string.h>
#include "tusb.h"
#include "pico/unique_id.h"
#include "usb_hid.h"

#define USB_VID   0x1209   // pid.codes
#define USB_PID   0x0002   // pid.codes test PID (private/hobby use)
#define USB_BCD   0x0200

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    // Composite with IAD (needed for the CDC log port)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0200,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

static uint8_t const desc_hid_keyboard[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

static uint8_t const desc_hid_mouse[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(RID_MOUSE)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(RID_CONSUMER))
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    return instance == ITF_KEYBOARD ? desc_hid_keyboard : desc_hid_mouse;
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + 2 * TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)
#define EPNUM_KEYBOARD   0x81
#define EPNUM_MOUSE      0x82
#define EPNUM_CDC_NOTIF  0x83
#define EPNUM_CDC_OUT    0x04
#define EPNUM_CDC_IN     0x84

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_KEYBOARD, 4, HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(desc_hid_keyboard), EPNUM_KEYBOARD,
                       CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(ITF_MOUSE, 5, HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_mouse), EPNUM_MOUSE,
                       CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_CDC_DESCRIPTOR(ITF_CDC, 6, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

static char const *string_desc_arr[] = {
    NULL,                         // 0: language (handled below)
    "PicoKeys",                   // 1: Manufacturer
    "PicoKeys Wireless Keyboard", // 2: Product
    NULL,                         // 3: Serial (board unique ID)
    "PicoKeys Keyboard",          // 4: Keyboard interface
    "PicoKeys Touchpad",          // 5: Mouse interface
    "PicoKeys Log",               // 6: Serial log port
};

static uint16_t _desc_str[33];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    uint8_t chr_count;
    char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];

    if (index == 0) {
        _desc_str[1] = 0x0409;
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        const char *str = string_desc_arr[index];
        if (index == 3) {
            pico_get_unique_board_id_string(serial, sizeof(serial));
            str = serial;
        }
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 32) chr_count = 32;
        for (uint8_t i = 0; i < chr_count; i++) _desc_str[1 + i] = (uint16_t)str[i];
    }
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
