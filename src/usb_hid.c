// HID report generation, event queues and host-OS fingerprinting.
#include <string.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "device/dcd.h"
#include "usb_hid.h"

// ---------------------------------------------------------------------------
// Event queues. Every key/button transition is queued and sent as its own
// report so fast taps (down+up arriving together over Wi-Fi) are never lost.
// ---------------------------------------------------------------------------
typedef enum {
    EV_KEY_DOWN = 1, EV_KEY_UP, EV_KB_RELEASE,
    EV_BUTTONS, EV_CONS_DOWN, EV_CONS_UP, EV_MS_RELEASE,
} ev_type_t;

typedef struct { uint8_t type; uint8_t a; uint16_t b; } hid_ev_t;

#define QLEN 64
typedef struct { hid_ev_t ev[QLEN]; uint8_t head, tail; } evq_t;

static evq_t kbq, msq;

static bool q_push(evq_t *q, hid_ev_t e) {
    uint8_t next = (uint8_t)((q->head + 1) % QLEN);
    if (next == q->tail) return false;   // full: drop
    q->ev[q->head] = e;
    q->head = next;
    return true;
}
static bool q_pop(evq_t *q, hid_ev_t *e) {
    if (q->head == q->tail) return false;
    *e = q->ev[q->tail];
    q->tail = (uint8_t)((q->tail + 1) % QLEN);
    return true;
}
static bool q_empty(const evq_t *q) { return q->head == q->tail; }
static void q_clear(evq_t *q) { q->head = q->tail = 0; }

// Current host-visible state
static uint8_t kb_mods;
static uint8_t kb_keys[6];
static uint8_t ms_buttons;
static int32_t acc_x, acc_y, acc_wheel, acc_pan;
static uint8_t kb_leds;

// "Logical" state as requested by the phone (used for the safety timeout)
static uint8_t want_keys_held;   // count of keys the phone says are down
static uint8_t want_buttons;
static bool want_consumer;

static void kick_wakeup(void) {
    if (tud_suspended()) tud_remote_wakeup();
}

void hid_key_down(uint8_t k) {
    if (!tud_mounted()) return;
    if (q_push(&kbq, (hid_ev_t){EV_KEY_DOWN, k, 0}) && want_keys_held < 255) want_keys_held++;
    kick_wakeup();
}
void hid_key_up(uint8_t k) {
    if (!tud_mounted()) return;
    if (q_push(&kbq, (hid_ev_t){EV_KEY_UP, k, 0}) && want_keys_held) want_keys_held--;
}
void hid_mouse_move(int16_t dx, int16_t dy) {
    if (!tud_mounted()) return;
    acc_x += dx; acc_y += dy;
    if (acc_x > 2000) acc_x = 2000; if (acc_x < -2000) acc_x = -2000;
    if (acc_y > 2000) acc_y = 2000; if (acc_y < -2000) acc_y = -2000;
    kick_wakeup();
}
void hid_mouse_buttons(uint8_t mask) {
    if (!tud_mounted()) return;
    want_buttons = mask & 0x1F;
    q_push(&msq, (hid_ev_t){EV_BUTTONS, want_buttons, 0});
    kick_wakeup();
}
void hid_scroll(int8_t wheel, int8_t pan) {
    if (!tud_mounted()) return;
    acc_wheel += wheel; acc_pan += pan;
    if (acc_wheel > 500) acc_wheel = 500; if (acc_wheel < -500) acc_wheel = -500;
    if (acc_pan > 500) acc_pan = 500; if (acc_pan < -500) acc_pan = -500;
}
void hid_consumer_press(uint16_t usage) {
    if (!tud_mounted()) return;
    want_consumer = true;
    q_push(&msq, (hid_ev_t){EV_CONS_DOWN, 0, usage});
    kick_wakeup();
}
void hid_consumer_release(void) {
    if (!tud_mounted()) return;
    want_consumer = false;
    q_push(&msq, (hid_ev_t){EV_CONS_UP, 0, 0});
}
void hid_release_all(void) {
    want_keys_held = 0; want_buttons = 0; want_consumer = false;
    acc_x = acc_y = acc_wheel = acc_pan = 0;
    if (!tud_mounted()) { q_clear(&kbq); q_clear(&msq); return; }
    // Make sure a release gets through even if the queues are full
    if (!q_push(&kbq, (hid_ev_t){EV_KB_RELEASE, 0, 0})) {
        q_clear(&kbq); q_push(&kbq, (hid_ev_t){EV_KB_RELEASE, 0, 0});
    }
    if (!q_push(&msq, (hid_ev_t){EV_MS_RELEASE, 0, 0})) {
        q_clear(&msq); q_push(&msq, (hid_ev_t){EV_MS_RELEASE, 0, 0});
    }
}
bool hid_anything_held(void) {
    return want_keys_held || want_buttons || want_consumer ||
           kb_mods || kb_keys[0] || ms_buttons;
}

static void kb_apply(const hid_ev_t *e) {
    uint8_t k = e->a;
    switch (e->type) {
    case EV_KEY_DOWN:
        if (k >= HID_KEY_CONTROL_LEFT && k <= HID_KEY_GUI_RIGHT) {
            kb_mods |= (uint8_t)(1u << (k - HID_KEY_CONTROL_LEFT));
        } else if (k) {
            for (int i = 0; i < 6; i++) if (kb_keys[i] == k) return;
            for (int i = 0; i < 6; i++) if (!kb_keys[i]) { kb_keys[i] = k; return; }
            // 6 keys already down: replace the oldest
            memmove(kb_keys, kb_keys + 1, 5);
            kb_keys[5] = k;
        }
        break;
    case EV_KEY_UP:
        if (k >= HID_KEY_CONTROL_LEFT && k <= HID_KEY_GUI_RIGHT) {
            kb_mods &= (uint8_t)~(1u << (k - HID_KEY_CONTROL_LEFT));
        } else {
            for (int i = 0; i < 6; i++) {
                if (kb_keys[i] == k) {
                    memmove(kb_keys + i, kb_keys + i + 1, (size_t)(5 - i));
                    kb_keys[5] = 0;
                    break;
                }
            }
        }
        break;
    case EV_KB_RELEASE:
        kb_mods = 0;
        memset(kb_keys, 0, sizeof(kb_keys));
        break;
    }
}

static int8_t take8(int32_t *acc) {
    int32_t v = *acc;
    if (v > 127) v = 127;
    if (v < -127) v = -127;
    *acc -= v;
    return (int8_t)v;
}

static bool send_mouse(void) {
    int8_t x = take8(&acc_x), y = take8(&acc_y);
    int8_t w = take8(&acc_wheel), p = take8(&acc_pan);
    return tud_hid_n_mouse_report(ITF_MOUSE, RID_MOUSE, ms_buttons, x, y, w, p);
}

static uint16_t cons_pending_release;   // EV_MS_RELEASE needs two reports

void hid_task(void) {
    if (!tud_mounted()) {
        q_clear(&kbq); q_clear(&msq);
        acc_x = acc_y = acc_wheel = acc_pan = 0;
        kb_mods = 0; memset(kb_keys, 0, sizeof(kb_keys)); ms_buttons = 0;
        return;
    }
    if (tud_suspended()) return;

    hid_ev_t e;
    if (tud_hid_n_ready(ITF_KEYBOARD) && q_pop(&kbq, &e)) {
        kb_apply(&e);
        tud_hid_n_keyboard_report(ITF_KEYBOARD, 0, kb_mods, kb_keys);
    }

    if (tud_hid_n_ready(ITF_MOUSE)) {
        if (cons_pending_release) {
            uint16_t zero = 0;
            tud_hid_n_report(ITF_MOUSE, RID_CONSUMER, &zero, 2);
            cons_pending_release = 0;
        } else if (q_pop(&msq, &e)) {
            switch (e.type) {
            case EV_BUTTONS:
                ms_buttons = e.a;
                send_mouse();
                break;
            case EV_CONS_DOWN:
                tud_hid_n_report(ITF_MOUSE, RID_CONSUMER, &e.b, 2);
                break;
            case EV_CONS_UP: {
                uint16_t zero = 0;
                tud_hid_n_report(ITF_MOUSE, RID_CONSUMER, &zero, 2);
                break;
            }
            case EV_MS_RELEASE:
                ms_buttons = 0;
                send_mouse();
                cons_pending_release = 1;
                break;
            }
        } else if (acc_x || acc_y || acc_wheel || acc_pan) {
            send_mouse();
        }
    }
}

// ---------------------------------------------------------------------------
// TinyUSB HID callbacks
// ---------------------------------------------------------------------------
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
    (void)report_id;
    if (instance == ITF_KEYBOARD && report_type == HID_REPORT_TYPE_OUTPUT && bufsize >= 1) {
        kb_leds = buffer[0];
    }
}

bool hid_usb_mounted(void) { return tud_mounted(); }
uint8_t hid_keyboard_leds(void) { return kb_leds; }

// ---------------------------------------------------------------------------
// Host OS fingerprinting.
// Each OS asks for USB string descriptors in a characteristic pattern
// (request lengths and order). This is a heuristic adapted from QMK's
// os_detection feature: it is usually right, but the web page always lets
// you override it.
// ---------------------------------------------------------------------------
static volatile uint8_t os_cnt, os_cnt_02, os_cnt_04, os_cnt_ff;
static volatile uint16_t os_last_len;
static volatile bool os_saw_msos;
static volatile uint32_t os_last_us;

void __real_dcd_event_handler(dcd_event_t const *event, bool in_isr);

void __not_in_flash_func(__wrap_dcd_event_handler)(dcd_event_t const *event, bool in_isr) {
    if (event->event_id == DCD_EVENT_SETUP_RECEIVED) {
        tusb_control_request_t const *r = &event->setup_received;
        if (r->bmRequestType == 0x80 && r->bRequest == TUSB_REQ_GET_DESCRIPTOR &&
            (r->wValue >> 8) == TUSB_DESC_STRING) {
            if ((r->wValue & 0xFF) == 0xEE) os_saw_msos = true;  // MS OS descriptor: Windows only
            if (os_cnt < 255) os_cnt++;
            os_last_len = r->wLength;
            if (r->wLength == 0x02 && os_cnt_02 < 255) os_cnt_02++;
            else if (r->wLength == 0x04 && os_cnt_04 < 255) os_cnt_04++;
            else if (r->wLength == 0xFF && os_cnt_ff < 255) os_cnt_ff++;
            os_last_us = time_us_32();
        }
    } else if (event->event_id == DCD_EVENT_UNPLUGGED) {
        os_cnt = os_cnt_02 = os_cnt_04 = os_cnt_ff = 0;
        os_saw_msos = false;
    }
    __real_dcd_event_handler(event, in_isr);
}

host_os_t hid_host_os(bool *confident) {
    *confident = false;
    if (!tud_mounted()) return HOST_OS_UNKNOWN;
    // wait until enumeration has settled
    if ((uint32_t)(time_us_32() - os_last_us) < 800000) return HOST_OS_UNKNOWN;

    uint8_t n = os_cnt, c2 = os_cnt_02, c4 = os_cnt_04, cf = os_cnt_ff;
    host_os_t os = HOST_OS_UNKNOWN;
    if (os_saw_msos) {
        os = HOST_OS_WINDOWS;
    } else if (n >= 3) {
        if (cf >= 2 && c4 >= 1)            os = HOST_OS_WINDOWS;
        else if (n == cf)                  os = HOST_OS_LINUX;
        else if (c2 >= 2 && cf <= 1)       os = HOST_OS_MACOS;   // also iPadOS
        else if (cf >= 2 && c2 == 0 && c4 == 0) os = HOST_OS_LINUX;
    }
    if (os != HOST_OS_UNKNOWN) *confident = true;
    return os;
}

const char *hid_host_os_name(host_os_t os) {
    switch (os) {
    case HOST_OS_WINDOWS: return "win";
    case HOST_OS_MACOS:   return "mac";
    case HOST_OS_LINUX:   return "linux";
    default:              return "unknown";
    }
}

void hid_init(void) {
    q_clear(&kbq); q_clear(&msq);
}
