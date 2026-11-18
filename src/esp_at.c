// ESP8285 AT-command driver (works with the old factory AT 1.2 firmware and
// newer 1.7.x builds: every command that changed name has a fallback).
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "hal.h"
#include "esp_at.h"

#define TX_HEAD_SIZE 1024
#define SEND_CHUNK   2048     // AT+CIPSEND limit
#define IPD_MAX      2100

typedef struct {
    bool open;
    uint8_t head[TX_HEAD_SIZE];
    uint16_t hlen;
    const uint8_t *body;
    uint32_t blen;
    bool close_req;
    bool close_sent;
    uint32_t close_ms;
    uint32_t gen;            // bumped on every reset, detects reuse mid-send
} link_t;

static link_t links[ESP_MAX_LINKS];
static esp_state_t state = ESP_STARTING;
static char version[64];
static uint32_t baud_now = 115200;
static bool dns_ok;
static bool verbose;
static uint32_t next_retry_ms;
static char saved_ssid[33], saved_pass[65];

// ---------------------------------------------------------------- RX parser
typedef enum { P_LINE, P_IPD_HDR, P_IPD_DATA } pstate_t;
static pstate_t pst = P_LINE;
static char line[200];
static uint16_t ll;
static uint8_t ipd_buf[IPD_MAX];
static int ipd_link;
static uint16_t ipd_len, ipd_got;

static volatile bool f_ok, f_error, f_prompt, f_send_ok, f_send_fail, f_busy, f_ready;
static bool awaiting_prompt;
static bool capture_lines;          // log every response line (init / raw cmds)

static void reset_flags(void) {
    f_ok = f_error = f_prompt = f_send_ok = f_send_fail = f_busy = false;
}

static void link_reset(int id) {
    link_t *L = &links[id];
    L->open = false; L->hlen = 0; L->body = NULL; L->blen = 0;
    L->close_req = L->close_sent = false;
    L->gen++;
}

static void handle_line(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ')) s[--n] = 0;
    if (!n) return;

    if (!strncmp(s, "AT version", 10) && !version[0]) {
        strncpy(version, s, sizeof(version) - 1);
    }
    if (!strcmp(s, "OK")) { f_ok = true; }
    else if (!strcmp(s, "ERROR") || !strcmp(s, "FAIL")) { f_error = true; }
    else if (!strcmp(s, "SEND OK")) { f_send_ok = true; }
    else if (!strcmp(s, "SEND FAIL")) { f_send_fail = true; }
    else if (!strncmp(s, "busy", 4)) { f_busy = true; }
    else if (!strcmp(s, "ready")) {
        f_ready = true;
        if (state == ESP_READY) {
            // module rebooted on its own: everything is gone, set up again
            log_printf("[esp] module restarted unexpectedly\n");
            for (int i = 0; i < ESP_MAX_LINKS; i++) {
                bool was = links[i].open;
                link_reset(i);
                if (was && i != ESP_DNS_LINK) net_on_closed(i);
            }
            state = ESP_STARTING;
            next_retry_ms = hal_ms() + 500;
        }
    } else if (s[0] >= '0' && s[0] <= '4' && s[1] == ',') {
        int id = s[0] - '0';
        const char *ev = s + 2;
        if (!strcmp(ev, "CONNECT")) {
            link_reset(id);
            links[id].open = true;
            net_on_connect(id);
        } else if (!strcmp(ev, "CLOSED") || !strcmp(ev, "CONNECT FAIL")) {
            bool was = links[id].open;
            link_reset(id);
            if (id == ESP_DNS_LINK && dns_ok) {
                dns_ok = false;
                log_printf("[esp] DNS socket closed\n");
            }
            if (was) net_on_closed(id);
        }
    }
    if (capture_lines || verbose) {
        if (strncmp(s, "Recv ", 5) != 0) log_printf("  < %s\n", s);
    }
}

static void rx_byte(uint8_t c) {
    switch (pst) {
    case P_IPD_DATA:
        if (ipd_got < IPD_MAX) ipd_buf[ipd_got] = c;
        ipd_got++;
        if (ipd_got >= ipd_len) {
            pst = P_LINE;
            uint16_t n = ipd_len < IPD_MAX ? ipd_len : IPD_MAX;
            if (ipd_link >= 0 && ipd_link < ESP_MAX_LINKS) {
                if (!links[ipd_link].open && ipd_link != ESP_DNS_LINK) {
                    // data before CONNECT was seen (rare): treat as connect
                    link_reset(ipd_link);
                    links[ipd_link].open = true;
                    net_on_connect(ipd_link);
                }
                net_on_data(ipd_link, ipd_buf, n);
            }
        }
        return;
    case P_IPD_HDR:
        if (ll < sizeof(line) - 1) line[ll++] = (char)c;
        if (c == ':') {
            line[ll] = 0;
            int id = -1, len = 0;
            // "+IPD,<id>,<len>[,<ip>,<port>]:"
            if (sscanf(line, "+IPD,%d,%d", &id, &len) == 2 && len > 0) {
                ipd_link = id; ipd_len = (uint16_t)len; ipd_got = 0;
                pst = P_IPD_DATA;
            } else {
                pst = P_LINE;
            }
            ll = 0;
        } else if (ll >= 40) {   // malformed
            pst = P_LINE; ll = 0;
        }
        return;
    case P_LINE:
        if (ll == 0 && c == '>' && awaiting_prompt) { f_prompt = true; return; }
        if (ll == 0 && (c == '\r' || c == '\n')) return;
        if (ll < sizeof(line) - 1) line[ll++] = (char)c;
        if (ll == 5 && !memcmp(line, "+IPD,", 5)) { pst = P_IPD_HDR; return; }
        if (c == '\n') {
            line[ll] = 0;
            handle_line(line);
            ll = 0;
        }
        return;
    }
}

static void rx_poll(void) {
    int c, n = 0;
    while (n < 4096 && (c = hal_uart_getc()) >= 0) { rx_byte((uint8_t)c); n++; }
}

// Keep USB alive and keep reading the module while we wait
static void wait_step(void) {
    rx_poll();
    hal_pump();
}

// ---------------------------------------------------------------- commands
static void uart_puts(const char *s) { hal_uart_write((const uint8_t *)s, strlen(s)); }

// returns 1 = OK, 0 = ERROR, -1 = timeout
static int at_cmd(uint32_t timeout_ms, const char *fmt, ...) {
    char buf[300];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (capture_lines || verbose) {
        // hide the hotspot password in the log
        const char *p = strstr(buf, "CWSAP");
        log_printf("  > %s\n", p ? "AT+CWSAP... (hidden)" : buf);
    }
    for (int attempt = 0; attempt < 3; attempt++) {
        reset_flags();
        uart_puts(buf);
        uart_puts("\r\n");
        uint32_t t0 = hal_ms();
        while (hal_ms() - t0 < timeout_ms) {
            wait_step();
            if (f_ok) return 1;
            if (f_error) return 0;
            if (f_busy) break;
        }
        if (!f_busy) return -1;
        // module busy: give it a moment then retry
        uint32_t t1 = hal_ms();
        while (hal_ms() - t1 < 100) wait_step();
    }
    return -1;
}

static bool wait_flag(volatile bool *flag, uint32_t timeout_ms) {
    uint32_t t0 = hal_ms();
    while (hal_ms() - t0 < timeout_ms) {
        wait_step();
        if (*flag) return true;
    }
    return false;
}

static bool at_send(int link, const uint8_t *data, uint16_t len) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%u\r\n", link, (unsigned)len);
    for (int attempt = 0; attempt < 3; attempt++) {
        reset_flags();
        awaiting_prompt = true;
        uart_puts(cmd);
        uint32_t t0 = hal_ms();
        while (hal_ms() - t0 < 2000 && !f_prompt && !f_error && !f_busy) wait_step();
        awaiting_prompt = false;
        if (f_prompt) {
            reset_flags();
            for (uint16_t off = 0; off < len; off += 64) {
                uint16_t n = (uint16_t)((len - off) < 64 ? (len - off) : 64);
                hal_uart_write(data + off, n);
                rx_poll();
                hal_pump();
            }
            uint32_t t1 = hal_ms();
            while (hal_ms() - t1 < 5000) {
                wait_step();
                if (f_send_ok) return true;
                if (f_send_fail || f_error) return false;
            }
            return false;
        }
        if (!f_busy) return false;
        uint32_t t2 = hal_ms();
        while (hal_ms() - t2 < 50) wait_step();
    }
    return false;
}

// Escape \ " , for use inside a quoted AT parameter
static void at_escape(char *out, size_t outlen, const char *in) {
    size_t o = 0;
    for (; *in && o + 2 < outlen; in++) {
        if (*in == '\\' || *in == '"' || *in == ',') out[o++] = '\\';
        out[o++] = *in;
    }
    out[o] = 0;
}

// Try the newer _CUR form first (does not wear the ESP flash), then the old one
static int at_cmd2(uint32_t to, const char *cur, const char *old) {
    int r = at_cmd(to, "%s", cur);
    if (r != 1) r = at_cmd(to, "%s", old);
    return r;
}

// ---------------------------------------------------------------- setup
static const uint32_t BAUDS[] = {115200, 9600, 57600, 74880, 230400, 460800, 921600};

static bool find_module(void) {
    for (size_t b = 0; b < sizeof(BAUDS) / sizeof(BAUDS[0]); b++) {
        hal_uart_set_baud(BAUDS[b]);
        baud_now = BAUDS[b];
        for (int t = 0; t < 2; t++) {
            hal_uart_flush_rx();
            pst = P_LINE; ll = 0;
            if (at_cmd(300, "AT") == 1) {
                log_printf("[esp] module answered at %lu baud\n", (unsigned long)BAUDS[b]);
                return true;
            }
        }
    }
    hal_uart_set_baud(115200);
    baud_now = 115200;
    return false;
}

void esp_start(const char *ssid, const char *pass) {
    strncpy(saved_ssid, ssid, sizeof(saved_ssid) - 1);
    strncpy(saved_pass, pass, sizeof(saved_pass) - 1);
    state = ESP_STARTING;
    dns_ok = false;
    capture_lines = true;
    for (int i = 0; i < ESP_MAX_LINKS; i++) {
        bool was = links[i].open;
        link_reset(i);
        if (was && i != ESP_DNS_LINK) net_on_closed(i);
    }
    log_printf("[esp] starting Wi-Fi module on GP0/GP1\n");

    if (!find_module()) {
        log_printf("[esp] no answer from the ESP8285 at any baud rate\n");
        state = ESP_NO_MODULE;
        capture_lines = false;
        next_retry_ms = hal_ms() + 10000;
        return;
    }

    // Restart the module so no old server/connections linger
    f_ready = false;
    at_cmd(1000, "AT+RST");
    wait_flag(&f_ready, 4000);
    { uint32_t t = hal_ms(); while (hal_ms() - t < 300) wait_step(); }
    hal_uart_flush_rx();
    pst = P_LINE; ll = 0;
    if (at_cmd(500, "AT") != 1 && !find_module()) {
        log_printf("[esp] module stopped answering after reset\n");
        state = ESP_NO_MODULE;
        capture_lines = false;
        next_retry_ms = hal_ms() + 10000;
        return;
    }

    at_cmd(500, "ATE0");                      // no command echo
    version[0] = 0;
    at_cmd(1000, "AT+GMR");

    int ok = at_cmd2(1000, "AT+CWMODE_CUR=2", "AT+CWMODE=2");   // hotspot only

    char es[70], ep[130], cmd[300];
    at_escape(es, sizeof(es), saved_ssid);
    at_escape(ep, sizeof(ep), saved_pass);
    if (ok == 1) {
        int r = -1;
        snprintf(cmd, sizeof(cmd), "AT+CWSAP_CUR=\"%s\",\"%s\",6,3", es, ep);
        r = at_cmd(5000, "%s", cmd);
        if (r != 1) {
            snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",6,3", es, ep);
            r = at_cmd(5000, "%s", cmd);
        }
        if (r != 1) {   // some builds only take WPA/WPA2 mixed
            snprintf(cmd, sizeof(cmd), "AT+CWSAP_CUR=\"%s\",\"%s\",6,4", es, ep);
            r = at_cmd(5000, "%s", cmd);
        }
        ok = r;
        if (ok != 1) log_printf("[esp] could not set the hotspot name/password\n");
    }
    if (ok == 1) {
        at_cmd2(1000, "AT+CWDHCP_CUR=0,1", "AT+CWDHCP=0,1");  // hand out addresses
        at_cmd(1000, "AT+CIPSERVER=0");                        // in case one is running
        ok = at_cmd(1000, "AT+CIPMUX=1");
        if (ok == 1) ok = at_cmd(2000, "AT+CIPSERVER=1,80");
        if (ok != 1) log_printf("[esp] could not start the web server\n");
    }
    if (ok == 1) {
        at_cmd(1000, "AT+CIPSTO=60");
        // UDP port 53, "remote may change" mode, so replies go to whoever asked
        dns_ok = at_cmd(3000, "AT+CIPSTART=%d,\"UDP\",\"192.168.4.2\",53,53,2", ESP_DNS_LINK) == 1;
        if (dns_ok) links[ESP_DNS_LINK].open = true;
        log_printf(dns_ok ? "[esp] captive-portal DNS on\n"
                          : "[esp] captive-portal DNS not supported by this firmware (use http://192.168.4.1)\n");
        state = ESP_READY;
        log_printf("[esp] hotspot \"%s\" is up - open http://192.168.4.1\n", saved_ssid);
    } else {
        state = ESP_SETUP_FAILED;
        next_retry_ms = hal_ms() + 10000;
    }
    capture_lines = false;
}

// ---------------------------------------------------------------- link API
bool esp_link_open(int link) { return link >= 0 && link < ESP_MAX_LINKS && links[link].open; }

uint16_t esp_tx_free(int link) {
    if (!esp_link_open(link)) return 0;
    return (uint16_t)(TX_HEAD_SIZE - links[link].hlen);
}

bool esp_write(int link, const void *data, uint16_t len) {
    if (!esp_link_open(link)) return false;
    link_t *L = &links[link];
    if (L->close_req || len > TX_HEAD_SIZE - L->hlen) return false;
    memcpy(L->head + L->hlen, data, len);
    L->hlen = (uint16_t)(L->hlen + len);
    return true;
}

void esp_write_static(int link, const uint8_t *data, uint32_t len) {
    if (!esp_link_open(link)) return;
    links[link].body = data;
    links[link].blen = len;
}

void esp_close_after(int link) {
    if (esp_link_open(link)) links[link].close_req = true;
}

void esp_close_now(int link) {
    if (!esp_link_open(link)) return;
    links[link].hlen = 0; links[link].blen = 0;
    links[link].close_req = true;
}

esp_state_t esp_state(void) { return state; }
const char *esp_version(void) { return version; }
uint32_t esp_baud(void) { return baud_now; }
bool esp_dns_enabled(void) { return dns_ok; }
void esp_set_verbose(bool v) { verbose = v; }

static char raw_pending[120];
void esp_raw_cmd(const char *s) {
    strncpy(raw_pending, s, sizeof(raw_pending) - 1);
    raw_pending[sizeof(raw_pending) - 1] = 0;
}

// ---------------------------------------------------------------- scheduler
static uint8_t send_buf[SEND_CHUNK];
static int rr;

void esp_task(void) {
    rx_poll();

    if (state != ESP_READY) {
        if ((int32_t)(hal_ms() - next_retry_ms) >= 0) esp_start(saved_ssid, saved_pass);
        return;
    }

    if (raw_pending[0]) {
        char cmd[120];
        strcpy(cmd, raw_pending);
        raw_pending[0] = 0;
        capture_lines = true;
        int r = at_cmd(5000, "%s", cmd);
        log_printf(r == 1 ? "[at] OK\n" : r == 0 ? "[at] ERROR\n" : "[at] (no reply)\n");
        capture_lines = false;
        return;
    }

    for (int k = 0; k < ESP_MAX_LINKS; k++) {
        int id = (rr + k) % ESP_MAX_LINKS;
        link_t *L = &links[id];
        if (!L->open) continue;

        if (L->hlen || L->blen) {
            uint16_t n = 0;
            uint16_t h = L->hlen < SEND_CHUNK ? L->hlen : SEND_CHUNK;
            memcpy(send_buf, L->head, h);
            n = h;
            uint32_t b = 0;
            if (id != ESP_DNS_LINK && n < SEND_CHUNK && L->blen) {
                b = SEND_CHUNK - n;
                if (b > L->blen) b = L->blen;
                memcpy(send_buf + n, L->body, b);
                n = (uint16_t)(n + b);
            }
            rr = (id + 1) % ESP_MAX_LINKS;
            uint32_t gen = L->gen;
            bool ok = at_send(id, send_buf, n);
            if (!L->open || L->gen != gen) return;  // closed/reused while sending
            if (ok) {
                if (L->hlen >= h) {
                    memmove(L->head, L->head + h, L->hlen - h);
                    L->hlen = (uint16_t)(L->hlen - h);
                } else L->hlen = 0;
                if (L->blen >= b) { L->body += b; L->blen -= b; } else L->blen = 0;
            } else if (id == ESP_DNS_LINK) {
                L->hlen = 0;                       // drop that reply
            } else {
                esp_close_now(id);
            }
            return;
        }
        if (L->close_req && !L->close_sent && id != ESP_DNS_LINK) {
            L->close_sent = true;
            L->close_ms = hal_ms();
            rr = (id + 1) % ESP_MAX_LINKS;
            int r = at_cmd(2000, "AT+CIPCLOSE=%d", id);
            if (r != 1 && L->open) {               // already gone
                link_reset(id);
                net_on_closed(id);
            }
            return;
        }
        if (L->close_sent && hal_ms() - L->close_ms > 3000) {   // never saw CLOSED
            link_reset(id);
            net_on_closed(id);
        }
    }
}
