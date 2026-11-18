// Tiny HTTP + WebSocket server running on the common network links (net.h).
//  GET /     -> the keyboard/touchpad page (gzip)
//  GET /ws   -> WebSocket carrying input events (binary) and status (JSON text)
//  UDP 53    -> answers every DNS name with 192.168.4.1 (captive portal)
#include <string.h>
#include <stdio.h>
#include <strings.h>
#include "hal.h"
#include "net.h"
#include "web_server.h"
#include "usb_hid.h"
#include "settings.h"
#include "sha1_b64.h"
#include "web_page.h"

#ifndef PICOKEYS_VERSION
#define PICOKEYS_VERSION "2.1"
#endif

#define RX_BUF 2048

typedef enum { C_FREE = 0, C_HTTP, C_WS } cstate_t;

typedef struct {
    cstate_t state;
    int link;
    uint8_t rx[RX_BUF];
    uint16_t rxlen;
    bool responded;
    uint32_t opened_ms;
} conn_t;

static conn_t conns[NET_MAX_LINKS];
static conn_t *ws_client;
static uint32_t ws_last_rx_ms;
static bool ws_released_on_timeout;
static bool reboot_req;
static uint32_t last_status_ms;
static char last_status[220];
static bool portal_done;     // the phone has loaded the page (captive portal satisfied)

static inline uint32_t now_ms(void) { return hal_ms(); }

bool web_client_connected(void) { return ws_client != NULL; }
bool web_reboot_requested(void) { return reboot_req; }

static bool default_password(void) { return strcmp(g_settings.pass, SETTINGS_DEFAULT_PASS) == 0; }

// While the default password is still set, keep the phone's sign-in window
// open so the "set your password" prompt isn't skipped.
static void portal_mark_ok(void) { if (!default_password()) portal_done = true; }

// ---------------------------------------------------------------------------
static void conn_reset(conn_t *c) {
    if (c == ws_client) {
        ws_client = NULL;
        hid_release_all();          // never leave keys stuck down
    }
    c->state = C_FREE;
    c->rxlen = 0;
    c->responded = false;
}

static void conn_close(conn_t *c) {
    net_close_now(c->link);
    conn_reset(c);
}

static void send_copy(conn_t *c, const void *data, uint16_t len) {
    net_write(c->link, data, len);
}

static void finish(conn_t *c) {
    c->responded = true;
    net_close_after(c->link);
}

static void send_simple(conn_t *c, const char *status, const char *ctype, const char *body) {
    char hdr[256];
    size_t bl = body ? strlen(body) : 0;
    int n = snprintf(hdr, sizeof(hdr),
                     "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     status, ctype, (unsigned)bl);
    send_copy(c, hdr, (uint16_t)n);
    if (bl) send_copy(c, body, (uint16_t)bl);
    finish(c);
}

static void send_redirect(conn_t *c) {
    static const char body[] = "<html><body><a href=\"http://192.168.4.1/\">Open PicoKeys</a></body></html>";
    char hdr[200];
    int n = snprintf(hdr, sizeof(hdr),
                     "HTTP/1.1 302 Found\r\nLocation: http://192.168.4.1/\r\n"
                     "Content-Type: text/html\r\nContent-Length: %u\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     (unsigned)(sizeof(body) - 1));
    send_copy(c, hdr, (uint16_t)n);
    send_copy(c, body, (uint16_t)(sizeof(body) - 1));
    finish(c);
}

static void send_page(conn_t *c) {
    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
                     "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                     "Content-Encoding: gzip\r\nContent-Length: %u\r\n"
                     "Cache-Control: no-cache\r\nConnection: close\r\n\r\n",
                     (unsigned)WEB_PAGE_GZ_LEN);
    send_copy(c, hdr, (uint16_t)n);
    net_write_static(c->link, WEB_PAGE_GZ, WEB_PAGE_GZ_LEN);
    finish(c);
}

// ---------------------------------------------------------------------------
// WebSocket
// ---------------------------------------------------------------------------
static void ws_send(conn_t *c, uint8_t opcode, const void *data, uint16_t len) {
    if (!c || c->state != C_WS) return;
    if (net_tx_free(c->link) < (uint16_t)(len + 4)) return;   // drop if congested
    uint8_t hdr[4];
    uint16_t hl;
    hdr[0] = (uint8_t)(0x80 | opcode);
    if (len < 126) { hdr[1] = (uint8_t)len; hl = 2; }
    else { hdr[1] = 126; hdr[2] = (uint8_t)(len >> 8); hdr[3] = (uint8_t)len; hl = 4; }
    send_copy(c, hdr, hl);
    if (len) send_copy(c, data, len);
}

static void build_status(char *out, size_t len) {
    bool confident;
    host_os_t os = hid_host_os(&confident);
    snprintf(out, len,
             "{\"t\":\"status\",\"usb\":%d,\"caps\":%d,\"num\":%d,\"os\":\"%s\",\"det\":%d,"
             "\"ssid\":\"%s\",\"def\":%d,\"v\":\"%s\"}",
             hid_usb_mounted() ? 1 : 0,
             (hid_keyboard_leds() & 0x02) ? 1 : 0,
             (hid_keyboard_leds() & 0x01) ? 1 : 0,
             hid_host_os_name(os), confident ? 1 : 0,
             g_settings.ssid, default_password() ? 1 : 0,
             PICOKEYS_VERSION);
}

static void ws_send_status(bool force) {
    if (!ws_client) return;
    char s[sizeof(last_status)];
    build_status(s, sizeof(s));
    // The serial link to the Wi-Fi chip is slow, so only resend on change
    // (plus a keep-alive every 3 s)
    if (force || strcmp(s, last_status) != 0 || now_ms() - last_status_ms > 3000) {
        ws_send(ws_client, 0x1, s, (uint16_t)strlen(s));
        strcpy(last_status, s);
        last_status_ms = now_ms();
    }
}

static bool json_ok_ssid(const char *s) {
    for (; *s; s++) if (*s == '"' || *s == '\\' || (unsigned char)*s < 0x20) return false;
    return true;
}

// Text commands: "W<ssid>\n<password>" save Wi-Fi, "R" factory reset Wi-Fi
static void ws_text(conn_t *c, const char *data, uint16_t len) {
    char msg[128];
    if (len > sizeof(msg) - 1) len = sizeof(msg) - 1;
    memcpy(msg, data, len);
    msg[len] = 0;
    if (msg[0] == 'W') {
        char *nl = strchr(msg + 1, '\n');
        bool ok = false;
        if (nl) {
            *nl = 0;
            const char *ssid = msg + 1, *pass = nl + 1;
            if (json_ok_ssid(ssid)) ok = settings_save(ssid, pass);
        }
        const char *r = ok ? "{\"t\":\"saved\",\"ok\":1}" : "{\"t\":\"saved\",\"ok\":0}";
        ws_send(c, 0x1, r, (uint16_t)strlen(r));
        if (ok) { log_printf("[web] new hotspot settings saved\n"); reboot_req = true; }
    } else if (msg[0] == 'R') {
        bool ok = settings_reset();
        const char *r = ok ? "{\"t\":\"saved\",\"ok\":1}" : "{\"t\":\"saved\",\"ok\":0}";
        ws_send(c, 0x1, r, (uint16_t)strlen(r));
        if (ok) reboot_req = true;
    } else if (msg[0] == 'S') {
        ws_send_status(true);
    }
}

// Binary input protocol (several commands may be packed in one frame):
//  00            heartbeat
//  01 kk         key down (HID usage)      02 kk   key up
//  03 xL xH yL yH  pointer move (int16 LE)
//  04 bb         mouse button mask
//  05 ww pp      scroll wheel / horizontal pan (int8)
//  06 uL uH      consumer (media) key press 07      consumer release
//  08            release everything
static void ws_binary(const uint8_t *p, uint16_t len) {
    uint16_t i = 0;
    while (i < len) {
        uint8_t op = p[i];
        switch (op) {
        case 0x00: i += 1; break;
        case 0x01: if (i + 2 > len) return; hid_key_down(p[i + 1]); i += 2; break;
        case 0x02: if (i + 2 > len) return; hid_key_up(p[i + 1]); i += 2; break;
        case 0x03:
            if (i + 5 > len) return;
            hid_mouse_move((int16_t)(p[i + 1] | p[i + 2] << 8), (int16_t)(p[i + 3] | p[i + 4] << 8));
            i += 5; break;
        case 0x04: if (i + 2 > len) return; hid_mouse_buttons(p[i + 1]); i += 2; break;
        case 0x05: if (i + 3 > len) return; hid_scroll((int8_t)p[i + 1], (int8_t)p[i + 2]); i += 3; break;
        case 0x06: if (i + 3 > len) return; hid_consumer_press((uint16_t)(p[i + 1] | p[i + 2] << 8)); i += 3; break;
        case 0x07: hid_consumer_release(); i += 1; break;
        case 0x08: hid_release_all(); i += 1; break;
        default: return;  // unknown: ignore rest
        }
    }
}

static void ws_process(conn_t *c) {
    while (c->state == C_WS && c->rxlen >= 2) {
        uint8_t *b = c->rx;
        uint8_t op = b[0] & 0x0F;
        bool masked = b[1] & 0x80;
        uint32_t plen = b[1] & 0x7F;
        uint32_t hl = 2;
        if (plen == 126) {
            if (c->rxlen < 4) return;
            plen = (uint32_t)b[2] << 8 | b[3];
            hl = 4;
        } else if (plen == 127) {
            conn_close(c);
            return;
        }
        if (masked) hl += 4;
        if (hl + plen > RX_BUF - 1) { conn_close(c); return; }
        if (c->rxlen < hl + plen) return;   // wait for the rest
        uint8_t *payload = b + hl;
        if (masked) {
            const uint8_t *m = b + hl - 4;
            for (uint32_t i = 0; i < plen; i++) payload[i] ^= m[i & 3];
        }
        ws_last_rx_ms = now_ms();
        ws_released_on_timeout = false;

        switch (op) {
        case 0x1: ws_text(c, (char *)payload, (uint16_t)plen); break;
        case 0x2: ws_binary(payload, (uint16_t)plen); break;
        case 0x8: ws_send(c, 0x8, NULL, 0); net_close_after(c->link); conn_reset(c); return;
        case 0x9: ws_send(c, 0xA, payload, (uint16_t)plen); break;
        default: break;
        }
        uint32_t used = hl + plen;
        memmove(c->rx, c->rx + used, c->rxlen - used);
        c->rxlen = (uint16_t)(c->rxlen - used);
    }
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------
static const char *find_header(const char *req, const char *name, size_t *vlen) {
    size_t nl = strlen(name);
    const char *p = strstr(req, "\r\n");
    while (p) {
        p += 2;
        if (p[0] == '\r' && p[1] == '\n') break;
        if (strncasecmp(p, name, nl) == 0 && p[nl] == ':') {
            const char *v = p + nl + 1;
            while (*v == ' ') v++;
            const char *e = strstr(v, "\r\n");
            if (!e) return NULL;
            *vlen = (size_t)(e - v);
            return v;
        }
        p = strstr(p, "\r\n");
    }
    return NULL;
}

static void http_upgrade(conn_t *c, const char *key, size_t klen) {
    static const char GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char buf[128];
    if (klen > 60) { conn_close(c); return; }
    memcpy(buf, key, klen);
    memcpy(buf + klen, GUID, sizeof(GUID) - 1);
    uint8_t dig[20];
    sha1((const uint8_t *)buf, klen + sizeof(GUID) - 1, dig);
    char acc[32];
    base64_encode(dig, 20, acc);

    char resp[200];
    int n = snprintf(resp, sizeof(resp),
                     "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                     "Connection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", acc);
    send_copy(c, resp, (uint16_t)n);

    // Only one controlling phone at a time: newest wins
    if (ws_client && ws_client != c) {
        conn_t *old = ws_client;
        ws_send(old, 0x8, NULL, 0);
        net_close_after(old->link);
        conn_reset(old);
    }
    c->state = C_WS;
    portal_mark_ok();
    ws_client = c;
    ws_last_rx_ms = now_ms();
    ws_released_on_timeout = false;
    last_status[0] = 0;
    ws_send_status(true);
    log_printf("[web] phone connected\n");
}

static void http_process(conn_t *c) {
    c->rx[c->rxlen] = 0;
    char *req = (char *)c->rx;
    char *end = strstr(req, "\r\n\r\n");
    if (!end) {
        if (c->rxlen >= RX_BUF - 1) conn_close(c);
        return;
    }
    size_t hdr_len = (size_t)(end - req) + 4;

    char method[8] = {0}, path[96] = {0};
    if (sscanf(req, "%7s %95s", method, path) != 2) { conn_close(c); return; }
    char *q = strchr(path, '?');
    if (q) *q = 0;

    size_t vlen;
    const char *up = find_header(req, "Upgrade", &vlen);
    if (strcmp(path, "/ws") == 0 && up && vlen >= 9 && strncasecmp(up, "websocket", 9) == 0) {
        const char *key = find_header(req, "Sec-WebSocket-Key", &vlen);
        if (!key) { send_simple(c, "400 Bad Request", "text/plain", "bad"); return; }
        http_upgrade(c, key, vlen);
        if (c->state != C_WS) return;
        uint16_t rest = (uint16_t)(c->rxlen - hdr_len);
        memmove(c->rx, c->rx + hdr_len, rest);
        c->rxlen = rest;
        ws_process(c);
        return;
    }

    char host[64] = {0};
    const char *hv = find_header(req, "Host", &vlen);
    if (hv) {
        size_t hl = vlen < sizeof(host) - 1 ? vlen : sizeof(host) - 1;
        memcpy(host, hv, hl);
        char *colon = strchr(host, ':');
        if (colon) *colon = 0;
    }
    c->rxlen = 0;
    bool for_us = !host[0] || !strcmp(host, "192.168.4.1") || !strcasecmp(host, "picokeys");

    // Connectivity probes from phones / laptops
    bool apple   = !strcmp(path, "/hotspot-detect.html") || !strncmp(path, "/library/test/success", 21);
    bool android = !strcmp(path, "/generate_204") || !strcmp(path, "/gen_204");
    bool win     = !strcmp(path, "/connecttest.txt") || !strcmp(path, "/ncsi.txt") || !strcmp(path, "/redirect");
    bool other   = !strcmp(path, "/success.txt") || !strcmp(path, "/canonical.html");

    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        send_simple(c, "405 Method Not Allowed", "text/plain", "405");
    } else if (apple || android || win || other) {
        if (!portal_done)     send_redirect(c);           // -> sign-in window pops up
        else if (apple)       send_simple(c, "200 OK", "text/html",
                                  "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
        else if (android)     send_simple(c, "204 No Content", "text/plain", NULL);
        else if (!strcmp(path, "/ncsi.txt")) send_simple(c, "200 OK", "text/plain", "Microsoft NCSI");
        else if (!strcmp(path, "/success.txt")) send_simple(c, "200 OK", "text/plain", "success\n");
        else                  send_simple(c, "200 OK", "text/plain", "Microsoft Connect Test");
    } else if (!for_us) {
        send_redirect(c);                                  // any other address -> PicoKeys
    } else if (!strcmp(path, "/") || !strcmp(path, "/index.html")) {
        portal_mark_ok();
        send_page(c);
        log_printf("[web] page requested\n");
    } else if (!strcmp(path, "/favicon.ico")) {
        send_simple(c, "204 No Content", "image/x-icon", NULL);
    } else {
        send_redirect(c);
    }
}

// ---------------------------------------------------------------------------
// Captive DNS: answer every A query with 192.168.4.1
// ---------------------------------------------------------------------------
static void dns_reply(const uint8_t *q, uint16_t len) {
    if (len < 12 || len > 400) return;
    if (q[2] & 0x80) return;                       // a response, not a query
    if (net_tx_free(NET_DNS_LINK) < 1024) return;  // previous reply still queued
    // find end of the first question name
    uint16_t i = 12;
    while (i < len && q[i]) {
        if ((q[i] & 0xC0) == 0xC0) { i++; break; }
        i = (uint16_t)(i + q[i] + 1);
    }
    i++;                                            // terminating zero
    if (i + 4 > len) return;
    uint16_t qtype = (uint16_t)(q[i] << 8 | q[i + 1]);
    uint16_t qend = (uint16_t)(i + 4);

    uint8_t r[460];
    memcpy(r, q, qend);
    r[2] = 0x81; r[3] = 0x80;                       // response, recursion available, no error
    r[4] = 0; r[5] = 1;                             // 1 question
    r[6] = 0; r[7] = (qtype == 1) ? 1 : 0;          // 1 answer for A records
    r[8] = r[9] = r[10] = r[11] = 0;
    uint16_t n = qend;
    if (qtype == 1) {
        static const uint8_t ans[] = {0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 192, 168, 4, 1};
        memcpy(r + n, ans, sizeof(ans));
        n = (uint16_t)(n + sizeof(ans));
    }
    net_write(NET_DNS_LINK, r, n);
}

// ---------------------------------------------------------------------------
// ESP link callbacks
// ---------------------------------------------------------------------------
void net_on_connect(int link) {
    if (link < 0 || link >= NET_MAX_LINKS) return;
    if (link == NET_DNS_LINK && net_dns_enabled()) return;
    conn_t *c = &conns[link];
    conn_reset(c);
    c->link = link;
    c->state = C_HTTP;
    c->opened_ms = now_ms();
}

void net_on_closed(int link) {
    if (link < 0 || link >= NET_MAX_LINKS) return;
    conn_t *c = &conns[link];
    if (c == ws_client) log_printf("[web] phone disconnected\n");
    conn_reset(c);
}

void net_on_data(int link, const uint8_t *d, uint16_t len) {
    if (link == NET_DNS_LINK && net_dns_enabled()) { dns_reply(d, len); return; }
    if (link < 0 || link >= NET_MAX_LINKS) return;
    conn_t *c = &conns[link];
    if (c->state == C_FREE) { net_on_connect(link); }
    uint16_t space = (uint16_t)(RX_BUF - 1 - c->rxlen);
    uint16_t n = len < space ? len : space;
    memcpy(c->rx + c->rxlen, d, n);
    c->rxlen = (uint16_t)(c->rxlen + n);
    if (c->state == C_HTTP) {
        if (!c->responded) http_process(c);
        else c->rxlen = 0;
    } else if (c->state == C_WS) {
        ws_process(c);
    }
}

void web_server_init(void) {
    for (int i = 0; i < NET_MAX_LINKS; i++) { conns[i].link = i; conns[i].state = C_FREE; }
}

void web_server_task(void) {
    // drop plain HTTP connections that hang around
    for (int i = 0; i < NET_MAX_LINKS; i++) {
        conn_t *c = &conns[i];
        if (c->state == C_HTTP && !c->responded && now_ms() - c->opened_ms > 10000) conn_close(c);
    }
    if (!ws_client) return;
    uint32_t idle = now_ms() - ws_last_rx_ms;
    // Phone went quiet (screen off, out of range...): let go of everything
    if (idle > 1500 && !ws_released_on_timeout && hid_anything_held()) {
        hid_release_all();
        ws_released_on_timeout = true;
    }
    if (idle > 20000) {
        log_printf("[web] phone timed out\n");
        conn_close(ws_client);
        return;
    }
    ws_send_status(false);
}
