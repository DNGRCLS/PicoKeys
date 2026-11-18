// Routes the web server's network calls to whichever Wi-Fi chip was found.
#include "net.h"
#include "esp_at.h"
#ifndef PICOKEYS_HOST_TEST
#include "cyw_net.h"
#endif

net_backend_t g_net_backend = NET_NONE;

#ifdef PICOKEYS_HOST_TEST
#define IS_CYW 0
#define CYW(call, dflt) (dflt)
#else
#define IS_CYW (g_net_backend == NET_CYW43)
#define CYW(call, dflt) (call)
#endif

void net_task(void) {
    if (IS_CYW) { CYW(cyw_task(), (void)0); }
    else if (g_net_backend == NET_ESP8285) esp_task();
}

net_state_t net_state(void) {
    if (IS_CYW) return CYW(cyw_ready(), false) ? NET_READY : NET_FAILED;
    if (g_net_backend != NET_ESP8285) return NET_STARTING;
    switch (esp_state()) {
    case ESP_READY: return NET_READY;
    case ESP_STARTING: return NET_STARTING;
    default: return NET_FAILED;
    }
}

const char *net_backend_name(void) {
    switch (g_net_backend) {
    case NET_CYW43: return "Raspberry Pi Pico W (CYW43439)";
    case NET_ESP8285: return "RP2040 + ESP8285 (Pico W-2023 clone)";
    default: return "detecting";
    }
}

bool net_write(int link, const void *d, uint16_t n) {
    return IS_CYW ? CYW(cyw_write(link, d, n), false) : esp_write(link, d, n);
}
uint16_t net_tx_free(int link) {
    return IS_CYW ? CYW(cyw_tx_free(link), 0) : esp_tx_free(link);
}
void net_write_static(int link, const uint8_t *d, uint32_t n) {
    if (IS_CYW) { CYW(cyw_write_static(link, d, n), (void)0); } else esp_write_static(link, d, n);
}
void net_close_after(int link) {
    if (IS_CYW) { CYW(cyw_close_after(link), (void)0); } else esp_close_after(link);
}
void net_close_now(int link) {
    if (IS_CYW) { CYW(cyw_close_now(link), (void)0); } else esp_close_now(link);
}
bool net_dns_enabled(void) {
    return IS_CYW ? true : esp_dns_enabled();
}
