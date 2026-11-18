// USB serial "PicoKeys log" port. Open it at any baud rate in the Arduino
// Serial Monitor (or PuTTY / screen) to see what the Wi-Fi module is doing.
// Commands: help, status, verbose, AT..., reboot, bootloader
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "tusb.h"
#include "hal.h"
#include "esp_at.h"
#include "net.h"
#include "settings.h"
#include "usb_hid.h"
#include "web_server.h"
#include "log.h"

#define LOG_SIZE 6144
static char logbuf[LOG_SIZE];
static uint32_t log_w;          // total bytes ever written
static uint32_t log_sent;       // total bytes sent to the terminal
static bool was_connected;
static bool verbose;

static void log_put(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\n') { logbuf[log_w % LOG_SIZE] = '\r'; log_w++; }
        logbuf[log_w % LOG_SIZE] = s[i];
        log_w++;
    }
}

void log_printf(const char *fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 1) n = sizeof(buf) - 1;
    log_put(buf, (size_t)n);
}

static const char *state_name(esp_state_t s) {
    switch (s) {
    case ESP_READY: return "hotspot running";
    case ESP_NO_MODULE: return "Wi-Fi module not answering";
    case ESP_SETUP_FAILED: return "Wi-Fi module setup failed";
    default: return "starting";
    }
}

static void print_status(void) {
    log_printf("\nPicoKeys %s\n", PICOKEYS_VERSION);
    log_printf("  Board        : %s\n", net_backend_name());
    if (g_net_backend == NET_ESP8285) {
        log_printf("  Wi-Fi module : %s\n", state_name(esp_state()));
        log_printf("  AT firmware  : %s\n", esp_version()[0] ? esp_version() : "(unknown)");
        log_printf("  UART         : GP0/GP1 at %lu baud\n", (unsigned long)esp_baud());
    } else if (g_net_backend == NET_CYW43) {
        log_printf("  Wi-Fi        : %s\n", net_state() == NET_READY ? "hotspot running" : "failed to start");
    }
    log_printf("  Hotspot      : \"%s\"%s\n", g_settings.ssid,
               strcmp(g_settings.pass, SETTINGS_DEFAULT_PASS) ? "" : " (default password)");
    log_printf("  Captive DNS  : %s\n", net_dns_enabled() ? "on" : "off");
    log_printf("  Phone        : %s\n", web_client_connected() ? "connected" : "not connected");
    log_printf("  USB keyboard : %s\n", hid_usb_mounted() ? "connected" : "not connected");
}

static void handle_cmd(char *s) {
    while (*s == ' ') s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\r')) s[--n] = 0;
    if (!n) return;
    if ((s[0] == 'A' || s[0] == 'a') && (s[1] == 'T' || s[1] == 't') && g_net_backend == NET_ESP8285) {
        s[0] = 'A'; s[1] = 'T';
        log_printf("[at] %s\n", s);
        esp_raw_cmd(s);
    } else if (!strcasecmp(s, "status")) {
        print_status();
    } else if (!strcasecmp(s, "verbose")) {
        verbose = !verbose;
        esp_set_verbose(verbose);
        log_printf("verbose AT logging %s\n", verbose ? "ON" : "OFF");
    } else if (!strcasecmp(s, "reboot")) {
        log_printf("rebooting...\n");
        watchdog_reboot(0, 0, 50);
    } else if (!strcasecmp(s, "bootloader")) {
        log_printf("entering USB bootloader (RPI-RP2 drive)...\n");
        sleep_ms(50);
        reset_usb_boot(0, 0);
    } else {
        log_printf("commands: status | verbose | AT<command> | reboot | bootloader\n");
    }
}

static char inbuf[128];
static uint16_t inlen;

void log_task(void) {
    if (!tud_mounted()) return;
    bool conn = tud_cdc_connected();
    if (conn && !was_connected) {
        // a terminal just opened: replay what we still have
        uint32_t oldest = log_w > LOG_SIZE ? log_w - LOG_SIZE : 0;
        log_sent = oldest;
        tud_cdc_write_str("\r\n=== PicoKeys log (type 'help') ===\r\n");
        print_status();
    }
    was_connected = conn;
    if (!conn) { log_sent = log_w; return; }

    if (log_w - log_sent > LOG_SIZE) log_sent = log_w - LOG_SIZE;
    while (log_sent < log_w) {
        uint32_t avail = tud_cdc_write_available();
        if (!avail) break;
        uint32_t idx = log_sent % LOG_SIZE;
        uint32_t n = log_w - log_sent;
        if (n > LOG_SIZE - idx) n = LOG_SIZE - idx;
        if (n > avail) n = avail;
        tud_cdc_write(logbuf + idx, n);
        log_sent += n;
    }
    tud_cdc_write_flush();

    while (tud_cdc_available()) {
        int c = tud_cdc_read_char();
        if (c < 0) break;
        if (c == '\r' || c == '\n') {
            inbuf[inlen] = 0;
            if (inlen) handle_cmd(inbuf);
            inlen = 0;
        } else if (inlen < sizeof(inbuf) - 1) {
            inbuf[inlen++] = (char)c;
        }
    }
}
