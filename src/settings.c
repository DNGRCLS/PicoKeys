// Wi-Fi hotspot name/password stored in the last 4 KB sector of flash.
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "pico/unique_id.h"
#include "hardware/flash.h"
#include "settings.h"

#define SETTINGS_MAGIC  0x504B4559u   // 'PKEY'
#define SETTINGS_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

typedef struct {
    uint32_t magic;
    settings_t s;
    uint32_t check;
} stored_t;

settings_t g_settings;

static uint32_t checksum(const settings_t *s) {
    const uint8_t *p = (const uint8_t *)s;
    uint32_t h = 2166136261u;                 // FNV-1a
    for (size_t i = 0; i < sizeof(*s); i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

void settings_default_ssid(char *out, int len) {
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    snprintf(out, (size_t)len, "PicoKeys-%02X%02X",
             id.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 2],
             id.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 1]);
}

void settings_load(void) {
    const stored_t *st = (const stored_t *)(XIP_BASE + SETTINGS_OFFSET);
    if (st->magic == SETTINGS_MAGIC && st->check == checksum(&st->s) &&
        st->s.ssid[0] && strlen(st->s.pass) >= 8 && strlen(st->s.pass) <= 63 &&
        memchr(st->s.ssid, 0, sizeof(st->s.ssid))) {
        memcpy(&g_settings, &st->s, sizeof(g_settings));
        return;
    }
    memset(&g_settings, 0, sizeof(g_settings));
    settings_default_ssid(g_settings.ssid, sizeof(g_settings.ssid));
    strcpy(g_settings.pass, SETTINGS_DEFAULT_PASS);
}

static uint8_t page_buf[FLASH_PAGE_SIZE * 2] __attribute__((aligned(4)));
static bool do_erase_only;

static void flash_op(void *param) {
    (void)param;
    flash_range_erase(SETTINGS_OFFSET, FLASH_SECTOR_SIZE);
    if (!do_erase_only) flash_range_program(SETTINGS_OFFSET, page_buf, sizeof(page_buf));
}

bool settings_save(const char *ssid, const char *pass) {
    size_t sl = strlen(ssid), pl = strlen(pass);
    if (sl < 1 || sl > 32 || pl < 8 || pl > 63) return false;
    stored_t st;
    memset(&st, 0, sizeof(st));
    st.magic = SETTINGS_MAGIC;
    memcpy(st.s.ssid, ssid, sl);
    memcpy(st.s.pass, pass, pl);
    st.check = checksum(&st.s);
    _Static_assert(sizeof(stored_t) <= sizeof(page_buf), "settings too big");
    memset(page_buf, 0xFF, sizeof(page_buf));
    memcpy(page_buf, &st, sizeof(st));
    do_erase_only = false;
    return flash_safe_execute(flash_op, NULL, 1000) == PICO_OK;
}

bool settings_reset(void) {
    do_erase_only = true;
    return flash_safe_execute(flash_op, NULL, 1000) == PICO_OK;
}
