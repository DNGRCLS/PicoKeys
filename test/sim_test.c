// Host-side test: runs the real esp_at.c + web_server.c against a simulated
// ESP8285 running AT 1.2-style firmware.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "hal.h"
#include "esp_at.h"
#include "net.h"
#include "web_server.h"
#include "settings.h"
#include "usb_hid.h"
#include "sha1_b64.h"

// ------------------------------------------------------------- stubs
settings_t g_settings = {"PicoKeys-AB12", "picokeys"};
bool settings_save(const char *s, const char *p) { strcpy(g_settings.ssid, s); strcpy(g_settings.pass, p); return true; }
bool settings_reset(void) { return true; }
static int keys_down[256], key_events, release_all_calls;
void hid_key_down(uint8_t k) { keys_down[k]++; key_events++; }
void hid_key_up(uint8_t k) { keys_down[k]--; key_events++; }
void hid_mouse_move(int16_t x, int16_t y) { (void)x; (void)y; }
void hid_mouse_buttons(uint8_t m) { (void)m; }
void hid_scroll(int8_t w, int8_t p) { (void)w; (void)p; }
void hid_consumer_press(uint16_t u) { (void)u; }
void hid_consumer_release(void) {}
void hid_release_all(void) { release_all_calls++; }
bool hid_anything_held(void) { return false; }
bool hid_usb_mounted(void) { return true; }
uint8_t hid_keyboard_leds(void) { return 0; }
host_os_t hid_host_os(bool *c) { *c = true; return HOST_OS_WINDOWS; }
const char *hid_host_os_name(host_os_t os) { (void)os; return "win"; }

// ------------------------------------------------------------- sim ESP
static uint32_t now;
static uint8_t to_rp[1 << 20]; static size_t to_rp_h, to_rp_t;
static uint32_t baud = 115200, sim_baud = 115200;
static int reject_cur = 1;        // old firmware: no _CUR commands
static int busy_once = 1;
static char cmdline[512]; static size_t cl;
static int sending_link = -1; static size_t send_need, send_got;
static uint8_t link_out[5][65536]; static size_t link_out_len[5];
static int link_closed[5];
static char log_cmds[8192];

uint32_t hal_ms(void) { return now; }
void hal_uart_init(uint32_t b) { baud = b; }
void hal_uart_set_baud(uint32_t b) { baud = b; }
void hal_uart_flush_rx(void) { to_rp_t = to_rp_h; }
int hal_uart_getc(void) { if (to_rp_t == to_rp_h) return -1; return to_rp[to_rp_t++]; }
void hal_pump(void) { now++; }
void log_printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }

static void reply(const char *s) { size_t n = strlen(s); memcpy(to_rp + to_rp_h, s, n); to_rp_h += n; }
static void reply_bin(const void *d, size_t n) { memcpy(to_rp + to_rp_h, d, n); to_rp_h += n; }

static void esp_command(char *c) {
    strcat(log_cmds, c); strcat(log_cmds, "\n");
    if (!strcmp(c, "AT") || !strcmp(c, "ATE0")) { reply("\r\nOK\r\n"); return; }
    if (!strcmp(c, "AT+RST")) { reply("\r\nOK\r\n\x01\xfe garbage \x80\r\nready\r\n"); return; }
    if (!strcmp(c, "AT+GMR")) { reply("AT version:1.2.0.0(Jul  1 2016 20:04:45)\r\nSDK version:1.5.4.1\r\nOK\r\n"); return; }
    if (strstr(c, "_CUR") && reject_cur) { reply("\r\nERROR\r\n"); return; }
    if (!strncmp(c, "AT+CWMODE", 9) || !strncmp(c, "AT+CWSAP", 8) || !strncmp(c, "AT+CWDHCP", 9) ||
        !strncmp(c, "AT+CIPMUX", 9) || !strncmp(c, "AT+CIPSERVER=1", 14) || !strncmp(c, "AT+CIPSTO", 9)) { reply("\r\nOK\r\n"); return; }
    if (!strcmp(c, "AT+CIPSERVER=0")) { reply("\r\nERROR\r\n"); return; }   // none running
    if (!strncmp(c, "AT+CIPSTART=4,\"UDP\"", 19)) { reply("4,CONNECT\r\n\r\nOK\r\n"); return; }
    if (!strncmp(c, "AT+CIPSEND=", 11)) {
        int l; unsigned n; sscanf(c + 11, "%d,%u", &l, &n);
        if (busy_once) { busy_once = 0; reply("busy p...\r\n"); return; }
        sending_link = l; send_need = n; send_got = 0;
        reply("\r\nOK\r\n> ");
        return;
    }
    if (!strncmp(c, "AT+CIPCLOSE=", 12)) {
        int l = atoi(c + 12); link_closed[l] = 1;
        char b[32]; snprintf(b, sizeof b, "%d,CLOSED\r\n\r\nOK\r\n", l); reply(b); return;
    }
    reply("\r\nERROR\r\n");
}

void hal_uart_write(const uint8_t *d, size_t n) {
    if (baud != sim_baud) return;  // wrong baud: module hears garbage
    for (size_t i = 0; i < n; i++) {
        if (sending_link >= 0) {
            link_out[sending_link][link_out_len[sending_link]++] = d[i];
            if (++send_got == send_need) {
                char b[48]; snprintf(b, sizeof b, "\r\nRecv %u bytes\r\n\r\nSEND OK\r\n", (unsigned)send_need);
                reply(b); sending_link = -1;
            }
            continue;
        }
        if (d[i] == '\n') { if (cl && cmdline[cl - 1] == '\r') cl--; cmdline[cl] = 0; esp_command(cmdline); cl = 0; }
        else if (cl < sizeof(cmdline) - 1) cmdline[cl++] = (char)d[i];
    }
}

static void client_send(int link, const void *d, size_t n) {
    char b[48]; snprintf(b, sizeof b, "+IPD,%d,%u:", link, (unsigned)n);
    reply(b); reply_bin(d, n);
}
static void run(int iters) { for (int i = 0; i < iters; i++) { net_task(); web_server_task(); now++; } }

static void ws_frame(int link, uint8_t op, const uint8_t *p, size_t n) {
    uint8_t f[200]; uint8_t m[4] = {0x12, 0x34, 0x56, 0x78};
    f[0] = 0x80 | op; f[1] = 0x80 | (uint8_t)n; memcpy(f + 2, m, 4);
    for (size_t i = 0; i < n; i++) f[6 + i] = p[i] ^ m[i & 3];
    client_send(link, f, n + 6);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1) sim_baud = (uint32_t)atoi(argv[1]);
    g_net_backend = NET_ESP8285;
    web_server_init();
    esp_start(g_settings.ssid, g_settings.pass);
    printf("state=%d version=%s baud=%u dns=%d\n", esp_state(), esp_version(), esp_baud(), esp_dns_enabled());
    assert(esp_state() == ESP_READY);
    assert(strstr(log_cmds, "AT+CWSAP=\"PicoKeys-AB12\",\"picokeys\",6,3"));
    assert(esp_dns_enabled());

    // 1) iPhone captive probe -> redirect
    reply("0,CONNECT\r\n");
    const char *probe = "GET /hotspot-detect.html HTTP/1.1\r\nHost: captive.apple.com\r\nUser-Agent: CaptiveNetworkSupport\r\n\r\n";
    client_send(0, probe, strlen(probe));
    run(50);
    link_out[0][link_out_len[0]] = 0;
    printf("probe reply: %.40s...\n", (char *)link_out[0]);
    assert(strstr((char *)link_out[0], "302 Found") && strstr((char *)link_out[0], "Location: http://192.168.4.1/"));
    assert(link_closed[0]);

    // 2) page
    reply("1,CONNECT\r\n");
    const char *get = "GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n";
    client_send(1, get, strlen(get));
    run(200);
    char *hdr_end = strstr((char *)link_out[1], "\r\n\r\n");
    assert(hdr_end);
    size_t body = link_out_len[1] - (size_t)(hdr_end + 4 - (char *)link_out[1]);
    unsigned clen = 0; sscanf(strstr((char *)link_out[1], "Content-Length: ") + 16, "%u", &clen);
    printf("page: content-length %u, body bytes received %zu, closed=%d\n", clen, body, link_closed[1]);
    assert(body == clen && link_closed[1]);
    FILE *f = fopen("page.gz", "wb"); fwrite(hdr_end + 4, 1, body, f); fclose(f);

    // 3) probe again: default password still set -> still redirect (keeps sign-in window)
    memset(link_closed, 0, sizeof link_closed); link_out_len[0] = 0;
    reply("0,CONNECT\r\n"); client_send(0, probe, strlen(probe)); run(50);
    link_out[0][link_out_len[0]] = 0;
    assert(strstr((char *)link_out[0], "302 Found"));

    // 4) WebSocket
    reply("2,CONNECT\r\n");
    const char *key = "dGhlIHNhbXBsZSBub25jZQ==";
    char up[300]; snprintf(up, sizeof up, "GET /ws HTTP/1.1\r\nHost: 192.168.4.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n", key);
    client_send(2, up, strlen(up));
    run(50);
    link_out[2][link_out_len[2]] = 0;
    assert(strstr((char *)link_out[2], "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
    assert(strstr((char *)link_out[2], "\"t\":\"status\"") && strstr((char *)link_out[2], "\"def\":1"));
    uint8_t kd[] = {1, 0x04, 2, 0x04, 0};
    ws_frame(2, 2, kd, sizeof kd);
    run(10);
    printf("key events=%d (a pressed then released)\n", key_events);
    assert(key_events == 2 && keys_down[4] == 0);

    // 5) change password over the socket
    const char *w = "WMyKeys\nsecret123";
    ws_frame(2, 1, (const uint8_t *)w, strlen(w));
    run(20);
    assert(web_reboot_requested() && !strcmp(g_settings.pass, "secret123"));
    link_out[2][link_out_len[2]] = 0;
    assert(memmem(link_out[2], link_out_len[2], "\"saved\",\"ok\":1", 14));

    // 6) captive probe once password is non-default and page loaded
    link_out_len[3] = 0; reply("3,CONNECT\r\n");
    client_send(3, get, strlen(get)); run(200);
    link_out_len[0] = 0; reply("0,CONNECT\r\n"); client_send(0, probe, strlen(probe)); run(50);
    link_out[0][link_out_len[0]] = 0;
    assert(strstr((char *)link_out[0], "Success"));

    // 7) DNS query for captive.apple.com
    uint8_t q[] = {0xab, 0xcd, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 0,
                   7, 'c','a','p','t','i','v','e', 5, 'a','p','p','l','e', 3, 'c','o','m', 0, 0, 1, 0, 1};
    client_send(4, q, sizeof q);
    run(20);
    uint8_t *r = link_out[4];
    printf("dns reply %zu bytes, id=%02x%02x answers=%d ip=%d.%d.%d.%d\n", link_out_len[4], r[0], r[1], r[7],
           r[link_out_len[4]-4], r[link_out_len[4]-3], r[link_out_len[4]-2], r[link_out_len[4]-1]);
    assert(link_out_len[4] == sizeof q + 16 && r[7] == 1 && r[link_out_len[4]-1] == 1 && r[link_out_len[4]-4] == 192);

    // 8) phone drops: everything released
    int before = release_all_calls;
    reply("2,CLOSED\r\n"); run(5);
    assert(!web_client_connected() && release_all_calls > before);

    printf("ALL TESTS PASSED (module at %u baud)\n", sim_baud);
    return 0;
}
