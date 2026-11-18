#pragma once
// Tiny hardware layer so the ESP8285 driver and web server can also be
// compiled and tested on a PC against a simulated module.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

uint32_t hal_ms(void);
void hal_uart_init(uint32_t baud);
void hal_uart_set_baud(uint32_t baud);
void hal_uart_write(const uint8_t *data, size_t len);
int  hal_uart_getc(void);          // -1 when nothing waiting
void hal_uart_flush_rx(void);
void hal_pump(void);               // keep USB etc. alive during waits
void log_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
