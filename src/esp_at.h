#pragma once
// Driver for the ESP8285 Wi-Fi co-processor on RP2040 "Pico W-2023" boards.
// The ESP runs Espressif AT-command firmware on UART0 (GP0 = TX to ESP,
// GP1 = RX from ESP). The RP2040 tells it to run a hotspot, a TCP server on
// port 80 and a UDP socket on port 53 (for the captive-portal DNS).
#include <stdint.h>
#include <stdbool.h>
#include "net.h"

#define ESP_MAX_LINKS 5
#define ESP_DNS_LINK  4

typedef enum {
    ESP_STARTING = 0,
    ESP_READY,
    ESP_NO_MODULE,      // nothing answered "AT"
    ESP_SETUP_FAILED,   // answered, but a required command failed
} esp_state_t;

void esp_start(const char *ssid, const char *pass);   // blocking setup
void esp_task(void);                                   // call from main loop
esp_state_t esp_state(void);
const char *esp_version(void);
uint32_t esp_baud(void);
bool esp_dns_enabled(void);

// Link I/O (link = connection id 0..4 reported by the module)
bool     esp_write(int link, const void *data, uint16_t len);   // copied
uint16_t esp_tx_free(int link);
void     esp_write_static(int link, const uint8_t *data, uint32_t len); // not copied
void     esp_close_after(int link);   // close once everything queued is sent
void     esp_close_now(int link);     // drop queued data and close
bool     esp_link_open(int link);

// Debug: run a raw AT command typed into the USB log port
void esp_raw_cmd(const char *line);
void esp_set_verbose(bool v);
