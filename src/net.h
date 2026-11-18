#pragma once
// Common network interface used by the web server. Two back ends:
//  - ESP8285 over AT commands (RP2040 "Pico W-2023" clones)
//  - Infineon CYW43439 + lwIP   (genuine Raspberry Pi Pico W)
// Links 0..3 are TCP connections on port 80; link 4 is the UDP DNS socket.
#include <stdint.h>
#include <stdbool.h>

#define NET_MAX_LINKS 5
#define NET_DNS_LINK  4

typedef enum { NET_NONE = 0, NET_ESP8285, NET_CYW43 } net_backend_t;
typedef enum { NET_STARTING = 0, NET_READY, NET_FAILED } net_state_t;

extern net_backend_t g_net_backend;

void        net_task(void);
net_state_t net_state(void);
const char *net_backend_name(void);

bool     net_write(int link, const void *data, uint16_t len);   // copied
uint16_t net_tx_free(int link);
void     net_write_static(int link, const uint8_t *data, uint32_t len);
void     net_close_after(int link);
void     net_close_now(int link);
bool     net_dns_enabled(void);

// Implemented by the web server, called by the back ends
void net_on_connect(int link);
void net_on_closed(int link);
void net_on_data(int link, const uint8_t *data, uint16_t len);
