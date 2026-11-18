#pragma once
#include <stdint.h>
#include <stdbool.h>

bool     cyw_probe(void);                       // true if a CYW43439 answers
void     cyw_start(const char *ssid, const char *pass);
void     cyw_task(void);
bool     cyw_ready(void);
void     cyw_led(bool on);

bool     cyw_write(int link, const void *data, uint16_t len);
uint16_t cyw_tx_free(int link);
void     cyw_write_static(int link, const uint8_t *data, uint32_t len);
void     cyw_close_after(int link);
void     cyw_close_now(int link);
