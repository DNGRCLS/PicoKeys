#pragma once
#include <stdint.h>
#include <stdbool.h>

enum { ITF_KEYBOARD = 0, ITF_MOUSE = 1, ITF_CDC = 2, ITF_CDC_DATA = 3, ITF_NUM_TOTAL };
enum { RID_MOUSE = 1, RID_CONSUMER = 2 };

typedef enum {
    HOST_OS_UNKNOWN = 0,
    HOST_OS_WINDOWS,
    HOST_OS_MACOS,
    HOST_OS_LINUX,
} host_os_t;

void hid_init(void);
void hid_task(void);

// Input events from the web page
void hid_key_down(uint8_t keycode);
void hid_key_up(uint8_t keycode);
void hid_mouse_move(int16_t dx, int16_t dy);
void hid_mouse_buttons(uint8_t mask);
void hid_scroll(int8_t wheel, int8_t pan);
void hid_consumer_press(uint16_t usage);
void hid_consumer_release(void);
void hid_release_all(void);
bool hid_anything_held(void);

// Status
bool hid_usb_mounted(void);
uint8_t hid_keyboard_leds(void);
host_os_t hid_host_os(bool *confident);
const char *hid_host_os_name(host_os_t os);
