// PicoKeys - USB keyboard + touchpad controlled from a phone over the
// board's own Wi-Fi hotspot. One firmware for both RP2040 boards:
//   - genuine Raspberry Pi Pico W (CYW43439 Wi-Fi chip)
//   - "Pico W-2023" clones (RP2040 + ESP8285 Wi-Fi chip on GP0/GP1)
// The Wi-Fi chip is detected at power-up.
//
//  Phone:  join Wi-Fi "PicoKeys-XXXX" (password "picokeys" until you change
//          it); the setup page pops up, or open http://192.168.4.1
//  LED:         fast flicker       = starting the Wi-Fi module
//               short blink / 2 s  = hotspot up, waiting for the phone
//               solid              = phone connected
//               double blink / 2 s = Wi-Fi module problem (see the log port)
//  Reset hotspot name/password: hold BOOTSEL for 5 s while running.
//  USB log port: open the "PicoKeys" serial port in a terminal, type help.
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "tusb.h"
#include "hal.h"
#include "esp_at.h"
#include "net.h"
#include "cyw_net.h"
#include "usb_hid.h"
#include "web_server.h"
#include "settings.h"
#include "log.h"

#define LED_PIN 25

// BOOTSEL button is wired to the flash chip-select; read it from RAM.
static bool __no_inline_not_in_flash_func(bootsel_pressed)(void) {
    const uint CS_PIN_INDEX = 1;
    uint32_t flags = save_and_disable_interrupts();
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (volatile int i = 0; i < 1000; ++i);
    bool pressed = !(sio_hw->gpio_hi_in & (1u << 1));
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    restore_interrupts(flags);
    return pressed;
}

static void led_set(bool on) {
    if (g_net_backend == NET_CYW43) cyw_led(on);     // Pico W: LED hangs off the Wi-Fi chip
    else gpio_put(LED_PIN, on);
}

static void led_update(void) {
    uint32_t t = hal_ms();
    bool on;
    switch (net_state()) {
    case NET_STARTING: on = (t % 200) < 100; break;
    case NET_READY:    on = web_client_connected() || (t % 2000) < 100; break;
    default:           { uint32_t p = t % 2000; on = p < 100 || (p >= 250 && p < 350); } break;
    }
    led_set(on);
}

// Called from every wait loop in the Wi-Fi driver, so USB never stalls
void hal_pump(void) {
    tud_task();
    hid_task();
    log_task();
    led_update();
}

static void reboot_soon(void) {
    uint32_t until = hal_ms() + 1500;   // let the "saved" reply reach the phone
    while (hal_ms() < until) { hal_pump(); net_task(); }
    hid_release_all();
    watchdog_reboot(0, 0, 0);
    while (1) tight_loop_contents();
}

int main(void) {
    hid_init();
    settings_load();
    web_server_init();

    // Which Wi-Fi chip is on this board? Try the genuine Pico W chip first
    // (takes ~0.1 s to rule out), otherwise use the ESP8285 on GP0/GP1.
    bool pico_w = cyw_probe();
    tusb_init();
    log_printf("\nPicoKeys %s\n", PICOKEYS_VERSION);

    if (pico_w) {
        g_net_backend = NET_CYW43;
        log_printf("[board] genuine Raspberry Pi Pico W (CYW43439)\n");
        for (int i = 0; i < 6; i++) { cyw_led(!(i & 1)); sleep_ms(120); }
        cyw_start(g_settings.ssid, g_settings.pass);
    } else {
        g_net_backend = NET_ESP8285;
        gpio_init(LED_PIN);
        gpio_set_dir(LED_PIN, GPIO_OUT);
        log_printf("[board] no CYW43439 - using the ESP8285 on GP0/GP1\n");
        hal_uart_init(115200);
        // give the ESP8285 time to finish its own boot before talking to it
        { uint32_t t = hal_ms(); while (hal_ms() - t < 800) hal_pump(); }
        esp_start(g_settings.ssid, g_settings.pass);
    }

    uint32_t last_btn_check = 0, btn_down_since = 0;
    bool btn_was_down = false;

    while (true) {
        hal_pump();
        net_task();
        web_server_task();

        uint32_t now = hal_ms();
        // BOOTSEL held 5 s -> reset hotspot name/password to defaults
        if (now - last_btn_check >= 100) {
            last_btn_check = now;
            bool down = bootsel_pressed();
            if (down && !btn_was_down) btn_down_since = now;
            btn_was_down = down;
            if (down && now - btn_down_since >= 5000) {
                log_printf("BOOTSEL held: resetting hotspot settings\n");
                for (int i = 0; i < 10; i++) { led_set(i & 1); sleep_ms(80); }
                settings_reset();
                reboot_soon();
            }
        }
        if (web_reboot_requested()) reboot_soon();
    }
}
