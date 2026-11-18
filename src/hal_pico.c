// RP2040 side of hal.h: UART0 on GP0 (TX -> ESP RX) / GP1 (RX <- ESP TX),
// with an interrupt-driven receive buffer so nothing is lost while busy.
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hal.h"

#define ESP_UART     uart0
#define ESP_TX_PIN   0
#define ESP_RX_PIN   1
#define RXQ_SIZE     8192

static volatile uint8_t rxq[RXQ_SIZE];
static volatile uint16_t rx_head, rx_tail;

static void __not_in_flash_func(uart_rx_isr)(void) {
    while (uart_is_readable(ESP_UART)) {
        uint8_t c = (uint8_t)uart_get_hw(ESP_UART)->dr;
        uint16_t next = (uint16_t)((rx_head + 1) % RXQ_SIZE);
        if (next != rx_tail) { rxq[rx_head] = c; rx_head = next; }
    }
}

uint32_t hal_ms(void) { return to_ms_since_boot(get_absolute_time()); }

void hal_uart_init(uint32_t baud) {
    uart_init(ESP_UART, baud);
    gpio_set_function(ESP_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(ESP_RX_PIN, GPIO_FUNC_UART);
    gpio_pull_up(ESP_RX_PIN);
    uart_set_hw_flow(ESP_UART, false, false);
    uart_set_format(ESP_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(ESP_UART, true);
    irq_set_exclusive_handler(UART0_IRQ, uart_rx_isr);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(ESP_UART, true, false);
}

void hal_uart_set_baud(uint32_t baud) {
    uart_tx_wait_blocking(ESP_UART);
    uart_set_baudrate(ESP_UART, baud);
}

void hal_uart_write(const uint8_t *data, size_t len) {
    uart_write_blocking(ESP_UART, data, len);
}

int hal_uart_getc(void) {
    if (rx_tail == rx_head) return -1;
    uint8_t c = rxq[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1) % RXQ_SIZE);
    return c;
}

void hal_uart_flush_rx(void) {
    uint32_t s = save_and_disable_interrupts();
    rx_tail = rx_head;
    restore_interrupts(s);
}
