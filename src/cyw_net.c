// Genuine Raspberry Pi Pico W back end: CYW43439 hotspot + lwIP (polled).
// Presents the same "link" interface as the ESP8285 driver.
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/ip4_addr.h"
#include "dhcpserver.h"
#include "net.h"
#include "cyw_net.h"
#include "hal.h"

#define TCP_LINKS   4            // links 0..3 (4 = DNS)
#define HEAD_SIZE   2048

typedef struct {
    struct tcp_pcb *pcb;
    uint8_t head[HEAD_SIZE];
    uint16_t hlen;
    const uint8_t *body;
    uint32_t blen;
    bool close_req;
    uint32_t opened_ms;
} clink_t;

static clink_t L[TCP_LINKS];
static bool ready;
static dhcp_server_t dhcp;
static struct udp_pcb *dns_pcb;
static ip_addr_t dns_peer;
static u16_t dns_peer_port;
static bool led_state = false, led_known = false;

bool cyw_probe(void) {
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_CANADA) != 0) return false;
    bool v;
    // Forces the driver to power up and talk to the chip; fails in ~0.1 s if absent
    if (cyw43_gpio_get(&cyw43_state, CYW43_WL_GPIO_VBUS_PIN, &v) != 0) {
        cyw43_arch_deinit();
        // hand the pins back (GP25 is the LED on clone boards)
        gpio_init(23); gpio_init(24); gpio_init(25); gpio_init(29);
        return false;
    }
    return true;
}

void cyw_led(bool on) {
    if (led_known && on == led_state) return;
    led_known = true; led_state = on;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

bool cyw_ready(void) { return ready; }

// ---------------------------------------------------------------- TCP
static void link_free(int id, bool notify) {
    clink_t *c = &L[id];
    c->pcb = NULL; c->hlen = 0; c->body = NULL; c->blen = 0; c->close_req = false;
    if (notify) net_on_closed(id);
}

static void pcb_detach(struct tcp_pcb *pcb) {
    tcp_arg(pcb, NULL); tcp_recv(pcb, NULL); tcp_sent(pcb, NULL);
    tcp_err(pcb, NULL); tcp_poll(pcb, NULL, 0);
}

// returns true if the pcb had to be aborted (callers inside lwIP callbacks
// must then return ERR_ABRT)
static bool do_close(int id) {
    clink_t *c = &L[id];
    bool aborted = false;
    if (c->pcb) {
        pcb_detach(c->pcb);
        if (tcp_close(c->pcb) != ERR_OK) { tcp_abort(c->pcb); aborted = true; }
    }
    link_free(id, true);
    return aborted;
}

// Write queued data. Closing is only done from the main loop (allow_close),
// never from inside an lwIP callback.
static void pump(int id, bool allow_close) {
    clink_t *c = &L[id];
    if (!c->pcb) return;
    bool wrote = false;
    while (c->hlen) {
        u16_t avail = tcp_sndbuf(c->pcb);
        if (!avail) break;
        u16_t n = c->hlen < avail ? c->hlen : avail;
        if (tcp_write(c->pcb, c->head, n, TCP_WRITE_FLAG_COPY) != ERR_OK) break;
        memmove(c->head, c->head + n, c->hlen - n);
        c->hlen = (uint16_t)(c->hlen - n);
        wrote = true;
    }
    while (!c->hlen && c->blen) {
        u16_t avail = tcp_sndbuf(c->pcb);
        if (!avail) break;
        uint32_t n = c->blen < avail ? c->blen : avail;
        // the page lives in flash and never changes: no copy needed
        if (tcp_write(c->pcb, c->body, (u16_t)n, 0) != ERR_OK) break;
        c->body += n; c->blen -= n;
        wrote = true;
    }
    if (wrote) tcp_output(c->pcb);
    if (allow_close && c->close_req && !c->hlen && !c->blen) do_close(id);   // FIN follows the data
}

static err_t on_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    (void)pcb; (void)len;
    pump((int)(intptr_t)arg, false);
    return ERR_OK;
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    int id = (int)(intptr_t)arg;
    if (!p || err != ERR_OK) {
        if (p) pbuf_free(p);
        return do_close(id) ? ERR_ABRT : ERR_OK;
    }
    tcp_recved(pcb, p->tot_len);
    uint8_t buf[512];
    uint16_t off = 0;
    while (off < p->tot_len) {
        uint16_t n = (uint16_t)pbuf_copy_partial(p, buf, sizeof(buf), off);
        if (!n) break;
        off = (uint16_t)(off + n);
        net_on_data(id, buf, n);
        if (L[id].pcb != pcb) break;        // closed while handling
    }
    pbuf_free(p);
    return ERR_OK;
}

static void on_err(void *arg, err_t err) {
    (void)err;
    int id = (int)(intptr_t)arg;
    link_free(id, true);                    // lwIP already freed the pcb
}

static err_t on_poll(void *arg, struct tcp_pcb *pcb) {
    (void)pcb;
    pump((int)(intptr_t)arg, false);
    return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || !pcb) return ERR_VAL;
    int id = -1;
    for (int i = 0; i < TCP_LINKS; i++) if (!L[i].pcb) { id = i; break; }
    if (id < 0) {
        // all busy: recycle the oldest one
        id = 0;
        for (int i = 1; i < TCP_LINKS; i++) if (L[i].opened_ms < L[id].opened_ms) id = i;
        struct tcp_pcb *old = L[id].pcb;
        pcb_detach(old);
        tcp_abort(old);
        link_free(id, true);
    }
    L[id].pcb = pcb;
    L[id].opened_ms = hal_ms();
    tcp_arg(pcb, (void *)(intptr_t)id);
    tcp_recv(pcb, on_recv);
    tcp_sent(pcb, on_sent);
    tcp_err(pcb, on_err);
    tcp_poll(pcb, on_poll, 2);
    tcp_nagle_disable(pcb);
    net_on_connect(id);
    return ERR_OK;
}

// ---------------------------------------------------------------- DNS (UDP 53)
static void on_dns(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    (void)arg; (void)pcb;
    uint8_t buf[512];
    uint16_t n = (uint16_t)pbuf_copy_partial(p, buf, sizeof(buf), 0);
    pbuf_free(p);
    ip_addr_copy(dns_peer, *addr);
    dns_peer_port = port;
    net_on_data(NET_DNS_LINK, buf, n);
}

// ---------------------------------------------------------------- link API
bool cyw_write(int link, const void *d, uint16_t n) {
    if (link == NET_DNS_LINK) {
        if (!dns_pcb || !dns_peer_port) return false;
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM);
        if (!p) return false;
        memcpy(p->payload, d, n);
        udp_sendto(dns_pcb, p, &dns_peer, dns_peer_port);
        pbuf_free(p);
        return true;
    }
    if (link < 0 || link >= TCP_LINKS || !L[link].pcb || L[link].close_req) return false;
    clink_t *c = &L[link];
    if (n > HEAD_SIZE - c->hlen) return false;
    memcpy(c->head + c->hlen, d, n);
    c->hlen = (uint16_t)(c->hlen + n);
    return true;
}

uint16_t cyw_tx_free(int link) {
    if (link == NET_DNS_LINK) return 1024;
    if (link < 0 || link >= TCP_LINKS || !L[link].pcb) return 0;
    return (uint16_t)(HEAD_SIZE - L[link].hlen);
}

void cyw_write_static(int link, const uint8_t *d, uint32_t n) {
    if (link < 0 || link >= TCP_LINKS || !L[link].pcb) return;
    L[link].body = d; L[link].blen = n;
}

void cyw_close_after(int link) {
    if (link < 0 || link >= TCP_LINKS || !L[link].pcb) return;
    L[link].close_req = true;
}

void cyw_close_now(int link) {
    if (link < 0 || link >= TCP_LINKS || !L[link].pcb) return;
    L[link].hlen = 0; L[link].blen = 0; L[link].close_req = true;
}

// ---------------------------------------------------------------- start / task
void cyw_start(const char *ssid, const char *pass) {
    cyw43_arch_enable_ap_mode(ssid, pass, CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t gw, mask;
    IP4_ADDR(&gw, 192, 168, 4, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    dhcp_server_init(&dhcp, &cyw43_state.netif[CYW43_ITF_AP], &gw, &mask);

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb && tcp_bind(pcb, IP_ANY_TYPE, 80) == ERR_OK) {
        struct tcp_pcb *lp = tcp_listen_with_backlog(pcb, 4);
        if (lp) { tcp_accept(lp, on_accept); ready = true; }
    }
    dns_pcb = udp_new();
    if (dns_pcb && udp_bind(dns_pcb, IP_ANY_TYPE, 53) == ERR_OK) udp_recv(dns_pcb, on_dns, NULL);
    else dns_pcb = NULL;

    log_printf(ready ? "[wifi] hotspot \"%s\" is up - open http://192.168.4.1\n"
                     : "[wifi] could not start the web server (%s)\n", ssid);
}

void cyw_task(void) {
    cyw43_arch_poll();
    for (int i = 0; i < TCP_LINKS; i++) if (L[i].pcb && (L[i].hlen || L[i].blen || L[i].close_req)) pump(i, true);
}
