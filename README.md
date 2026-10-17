# PicoKeys

**Turn a Raspberry Pi Pico W (or an RP2040 + ESP8285 "Pico W" clone) into a wireless keyboard and touchpad you control from your phone.**

Plug the board into any computer's USB port and it shows up as an ordinary USB keyboard and mouse. No drivers or computer-side software are needed. The board also runs its own Wi-Fi hotspot. Join it from your phone and a full keyboard and touchpad opens in the phone's browser. Everything you tap is sent over Wi-Fi to the board, which types it into the computer.

> Firmware file: **`PicoKeys_RP2040_PicoW+Clone.uf2`** · Version **2.1**

---

## Contents

- [Features](#features)
- [Supported boards](#supported-boards)
- [Quick start](#quick-start)
- [Using the phone page](#using-the-phone-page)
- [Status LED](#status-led)
- [The USB log port](#the-usb-log-port)
- [Changing or resetting the hotspot password](#changing-or-resetting-the-hotspot-password)
- [Computer OS detection](#computer-os-detection)
- [Troubleshooting](#troubleshooting)
- [Security notes](#security-notes)
- [Known limitations](#known-limitations)
- [How it works](#how-it-works)
- [Building from source](#building-from-source)
- [Project layout](#project-layout)
- [Testing](#testing)
- [Credits and licenses](#credits-and-licenses)

---

## Features

**On the computer**
- Shows up as a standard USB **keyboard** and a **mouse** with vertical and horizontal scroll. The keyboard supports the boot protocol, so it should also work in most BIOS/UEFI menus.
- **Media keys**: play/pause, next, previous, stop, mute, volume and brightness.
- Works with **Windows, macOS, Linux and ChromeOS**. No drivers.
- Tries to **detect the computer's operating system** and adapt key labels and shortcuts (⌘/⌥ on a Mac, Win/Super elsewhere).
- **Caps Lock state** from the computer is shown on the phone.
- Can **wake a sleeping computer** when you press a key, if the computer allows USB wake.
- Also provides a **USB serial log port** for diagnostics.

**On the phone**
- Full keyboard with a switchable top row: **F1–F12**, **Nav** (Home/End/PgUp/PgDn/Ins/Del…), **Media**, and per-OS **Shortcuts** (copy, paste, undo, app switcher, lock screen…).
- **Touchpad mode**:
  - Tap to click, two-finger tap to right-click, three-finger tap to middle-click.
  - Two-finger drag to scroll, and double-tap-and-drag to select or drag.
  - A scroll strip down the right edge, plus left/middle/right buttons.
- **Sticky modifiers**: tap Shift/Ctrl/Alt/Win once for the next key, tap twice to lock, tap again to release. You can also hold one with a finger.
- **Scales automatically** to any phone screen. It's designed for landscape and shows a short warning in portrait.
- **Captive portal**: join the hotspot and the page opens by itself on iPhone and Android.
- **First-run prompt** to replace the default hotspot password.
- Settings for pointer speed, scroll speed, natural scrolling, tap-to-click and vibration (Android). Settings are remembered on the phone.

**Safety**
- If the phone disconnects, locks or goes out of range, every key and button is **released within about 1.5 seconds**, so nothing gets stuck down.
- Only one phone controls the board at a time. The newest connection wins.

---

## Supported boards

One `.uf2` file supports both of these. The board type is detected automatically at power-up.

| Board | Processor | Wi-Fi chip | Supported |
|---|---|---|---|
| **Raspberry Pi Pico W** (genuine) | RP2040 | Infineon CYW43439 | ✅ |
| **"RP2040 Pico W-2023"** and similar clones | RP2040 | Espressif **ESP8285** on GP0/GP1 | ✅ |
| Raspberry Pi Pico 2 W | RP2350 | CYW43439 | ❌ Needs a separate RP2350 build |
| Raspberry Pi Pico (no Wi-Fi) | RP2040 | none | ❌ |

**Which one do I have?** On a genuine Pico W the Wi-Fi chip sits under a small metal shield and the board says "Raspberry Pi Pico W". The clones usually have an exposed **ESP8285** chip near the antenna and **"ESP RX / ESP TX"** printed next to GP0/GP1 on the back. When you hold BOOTSEL, both show a drive named **RPI-RP2**.

> **Clone boards:** GP0 and GP1 are wired to the ESP8285, so don't connect anything else to those two pins.

---

## Quick start

### 1. Flash the firmware
1. Hold the **BOOTSEL** button while plugging the board into your computer.
2. A drive called **RPI-RP2** appears.
3. Drag **`PicoKeys_RP2040_PicoW+Clone.uf2`** onto it.
4. The board restarts by itself. Your computer now sees a keyboard, a mouse and a serial port.

### 2. Connect your phone
1. On your phone, open the Wi-Fi settings and join **`PicoKeys-XXXX`**. The last 4 characters are unique to your board.
2. Enter the default password: **`picokeys`**
3. The PicoKeys page should **open automatically**. If it doesn't, open a browser and go to **http://192.168.4.1**.

### 3. Set your own password (first run)
PicoKeys asks you to replace the default password. Enter a new one twice and tap **Save & restart**. The board restarts its hotspot with the new password. Then:

- Rejoin `PicoKeys-XXXX` (or the new name you chose) with the **new** password.
- **iPhone:** if it keeps trying the old password, go to Settings → Wi-Fi, tap ⓘ next to the network, choose **Forget This Network**, then join again.

### 4. Type!
Turn the phone sideways and start typing or switch to **Touchpad**.

**iPhone tips**
- The page first opens in the iPhone's small Wi-Fi sign-in window. It works there, but for full screen:
  1. Tap **Done**, or **Cancel → Use Without Internet**, in the top right corner.
  2. Open **Safari** and go to **192.168.4.1**.
- In Safari, **Share → Add to Home Screen** gives you a full-screen PicoKeys app icon.

> While connected to PicoKeys your phone has no internet, because the hotspot isn't connected to anything else. Mobile data may still work for other apps depending on your phone.

---

## Using the phone page

### Top bar
| Control | What it does |
|---|---|
| **Keyboard / Touchpad** | Switch modes |
| **F1–F12 · Nav · Media · Shortcuts** | Choose what the top key row shows (keyboard mode) |
| **● Windows auto** | Connection status and the detected computer OS. Tap to open settings |
| **⇪ Caps** | Shown while Caps Lock is on (on the computer) |
| **⛶** | Full screen and lock to landscape. Android only; hidden on iPhone |
| **⚙** | Settings |

The status dot is **green** when everything is connected and **amber** when the phone is connected but the board isn't plugged into a computer. It's **red** when the phone can't reach the board.

### Modifier keys (Shift, Ctrl, Alt, Win/⌘)
- **Tap once:** applies to the next key only. The key shows an outline.
- **Tap twice:** locks on and is shown filled. Useful for Shift-click or Ctrl-scroll in touchpad mode.
- **Tap again:** releases it.
- **Hold:** keep a finger on it while pressing other keys, like a real keyboard.

Modifier state is shared between keyboard and touchpad modes.

### Shortcuts row
Shortcuts change with the computer's OS. Some examples:

| | Windows | macOS | Linux | ChromeOS |
|---|---|---|---|---|
| Copy / Paste | Ctrl+C / Ctrl+V | ⌘C / ⌘V | Ctrl+C / Ctrl+V | Ctrl+C / Ctrl+V |
| Redo | Ctrl+Y | ⇧⌘Z | Ctrl+⇧Z | Ctrl+⇧Z |
| App switcher | Alt+Tab | ⌘Tab | Alt+Tab | Alt+Tab |
| Launcher | Win (Start) | ⌘Space (Spotlight) | Super | Search |
| Lock | Win+L | ⌃⌘Q | Super+L | Search+L |

**App switcher:** tap it repeatedly to step through windows. It releases on its own after one second.

### Touchpad gestures
| Gesture | Action |
|---|---|
| One-finger drag | Move pointer (with acceleration) |
| Tap | Left click |
| Two-finger tap | Right click |
| Three-finger tap | Middle click |
| Two-finger drag | Scroll (vertical and horizontal) |
| Double-tap, then drag | Click-and-drag / select text |
| Drag on the right-edge strip | Scroll |
| **Left / Middle / Right** buttons | Press and hold like real mouse buttons |

### Settings (⚙)
- **Operating system:** Auto-detect, or force Windows / macOS / Linux / ChromeOS.
- **Pointer speed** and **Scroll speed**.
- **Natural scrolling:** content follows your fingers, like a phone.
- **Tap to click**.
- **Vibrate on key press:** Android only; iPhone browsers don't allow it.
- **Hotspot name and password:** change them and restart.
- **Reset to default:** puts the hotspot back to `PicoKeys-XXXX` / `picokeys`.

Phone-side settings (speeds, OS choice and so on) are saved in that phone's browser.

---

## Status LED

| Pattern | Meaning |
|---|---|
| 3 quick blinks at power-up | Genuine Pico W detected, Wi-Fi chip started |
| Fast flicker | Starting the Wi-Fi module (clone boards) |
| Short blink every 2 s | Hotspot running, waiting for a phone |
| Solid on | Phone connected |
| Double blink every 2 s | Wi-Fi module problem. Check the [log port](#the-usb-log-port) |
| Rapid blinking, then restart | Hotspot settings being reset (BOOTSEL held) |

On a genuine Pico W the LED is controlled by the Wi-Fi chip. On clones it is the normal LED on **GP25**.

---

## The USB log port

Along with the keyboard and mouse, PicoKeys adds a USB serial port called **"PicoKeys Log"**. Open it in any serial terminal, such as the **Arduino IDE Serial Monitor**, PuTTY, `screen` or `minicom`. Any baud rate works.

When you open it, PicoKeys replays what it has logged since power-up and prints a status summary. That's very handy for seeing what happened during startup.

| Command | What it does |
|---|---|
| `status` | Board type, Wi-Fi state, AT firmware version, hotspot name, phone/USB connection |
| `verbose` | Toggle logging of every command exchanged with the ESP8285 |
| `AT…` | *(Clone boards)* Send any AT command straight to the ESP8285 and show the reply, e.g. `AT+GMR` |
| `reboot` | Restart PicoKeys |
| `bootloader` | Restart into the RPI-RP2 USB drive for re-flashing, without pressing BOOTSEL |
| `help` | List commands |

The hotspot password is never printed in the log.

---

## Changing or resetting the hotspot password

- **Change it:** tap ⚙ → *PicoKeys hotspot*, enter a new name and/or password, then tap **Save & restart**.
- **Forgot it?** With the board running, **hold BOOTSEL for 5 seconds**. The LED blinks rapidly and PicoKeys restarts with the default name `PicoKeys-XXXX` and password `picokeys`.

Rules: the name can be 1–32 characters with no quotes or backslashes. The password must be 8–63 characters and can't be `picokeys`.

The settings are stored in the last 4 KB of the board's flash. **Re-flashing the UF2 keeps them.**

---

## Computer OS detection

Operating systems ask a USB device for its descriptors in slightly different ways. PicoKeys watches those requests while the computer sets it up and makes a best guess, using a method adapted from QMK's `os_detection` feature:

- **Windows** and **macOS** are usually identified correctly.
- **Linux** and **ChromeOS** look the same over USB, so ChromeOS shows as Linux. Choose ChromeOS in settings if you use one.
- If PicoKeys can't tell, it assumes **Windows**.

The detected OS and whether it was a confident guess appear in ⚙ → *Operating system*. You can always override it. PicoKeys always sends **US English** key positions, so set the computer's keyboard layout to US for symbols to come out as labelled.

---

## Troubleshooting

**I can't see the `PicoKeys-XXXX` network**
- Check the LED. A double blink means the Wi-Fi module failed. Open the [log port](#the-usb-log-port) and type `status`.
- On a clone, the module's own network (often named like `B&T_xxxxxx`) can appear for a second at power-up. PicoKeys renames it once it has configured the module.
- Make sure the board is a supported RP2040 board. A **Pico 2 W** won't run this file.

**The page doesn't pop up by itself**
- Open **http://192.168.4.1** in the browser yourself.
- On clones, some old ESP8285 firmware can't do the trick behind the automatic popup. The log port will say *"captive-portal DNS not supported"*. Everything else works normally.

**The page loads but says "Connecting to PicoKeys…"**
- Make sure the phone is still on the PicoKeys Wi-Fi. Some phones switch away from networks without internet.
- Reload the page.

**"Connected – plug PicoKeys into a computer"**
- The phone reached the board, but the board isn't connected to a computer over USB, or the cable is power-only. Use a data cable.

**Keys type the wrong symbols**
- Set the computer's keyboard layout to **US English**.

**Mac shows a "Keyboard Setup Assistant" window**
- That's normal for a new keyboard on macOS. Follow it, or close it.

**Re-flashing**
- Hold BOOTSEL while plugging in, or type `bootloader` in the log port, then drag the new UF2 onto RPI-RP2.

**Clone board: the log says the module isn't answering**
- PicoKeys tries 115200, 9600, 57600, 74880, 230400, 460800 and 921600 baud on GP0/GP1.
- If none answer, check that nothing else is connected to GP0/GP1, and power-cycle the board.

---

## Security notes

- Anyone who joins the hotspot can type on the connected computer. **Change the default password on first use.** The page will keep reminding you until you do.
- The hotspot uses WPA2. The web page and WebSocket are plain HTTP **inside that encrypted Wi-Fi link**. Keep the password private.
- Hotspot settings are stored unencrypted in the board's flash, so anyone with the physical board can read them.
- Unplug PicoKeys when you aren't using it.

---

## Known limitations

- **Pico 2 W (RP2350)** is not supported by this file.
- **US keyboard layout only.**
- **One phone at a time.**
- **Clone boards** talk to the ESP8285 over a 115200-baud serial link:
  - The page takes about 1–2 seconds to load the first time.
  - Typing and pointer movement are still responsive.
- **No internet** is passed through the hotspot.
- The keyboard reports up to **6 keys** at once (6-key rollover), plus modifiers.
- The USB vendor/product ID is **`1209:0002`**, a [pid.codes](https://pid.codes) *test* ID. That's fine for personal use. Get your own ID if you plan to sell or widely distribute devices.

---

## How it works

```
 ┌──────────┐   Wi-Fi (WPA2)    ┌───────────────────────────────┐   USB    ┌──────────┐
 │  Phone   │ ───────────────▶  │  RP2040                        │ ───────▶ │ Computer │
 │ browser  │  HTTP + WebSocket │  • web server + WebSocket      │  HID kbd │          │
 │          │ ◀───────────────  │  • TinyUSB: keyboard, mouse,   │  + mouse │          │
 └──────────┘                   │    media keys, log port        │  + CDC   └──────────┘
                                │  Wi-Fi chip (auto-detected):   │
                                │   • CYW43439 + lwIP  (Pico W)  │
                                │   • ESP8285 via AT (clone)     │
                                └───────────────────────────────┘
```

- **Detection:** at power-up the firmware asks the Pico W's CYW43439 Wi-Fi chip to respond. If it doesn't answer within about 0.1 s, the firmware falls back to the ESP8285 on UART0 (GP0 = TX, GP1 = RX).
- **Genuine Pico W:** the CYW43439 runs the hotspot and a DHCP server. lwIP serves TCP port 80 and a captive DNS server on UDP 53.
- **Clone:** the ESP8285 runs its factory AT-command firmware (tested against AT 1.2-style behaviour, with fallbacks for 1.7.x). PicoKeys sets it up as a WPA2 hotspot with a TCP server on port 80 and a UDP socket on port 53, then exchanges data with it using `AT+CIPSEND` and `+IPD`. Each command has an `_CUR` and a plain fallback, and the baud rate is detected automatically.
- **Captive portal:** every DNS name resolves to `192.168.4.1`, and phone "is there internet?" checks are redirected to the PicoKeys page. Once the phone has loaded the page (and the default password has been changed), those checks are answered as "online" so the phone stays connected.
- **Web page:** a single HTML/CSS/JS file. It's gzipped at build time and stored in flash.

### WebSocket protocol (`ws://192.168.4.1/ws`)
Binary frames from the phone; several commands may be packed into one frame:

| Bytes | Meaning |
|---|---|
| `00` | Heartbeat (sent every 500 ms; keys are released if these stop) |
| `01 kk` / `02 kk` | Key down / key up (HID usage code, `E0`–`E7` = modifiers) |
| `03 xL xH yL yH` | Pointer move, signed 16-bit little-endian |
| `04 bb` | Mouse button mask (1 = left, 2 = right, 4 = middle) |
| `05 ww pp` | Scroll wheel / horizontal pan, signed 8-bit |
| `06 uL uH` / `07` | Media (consumer) key press / release |
| `08` | Release everything |

Text frames: `W<name>\n<password>` saves new hotspot settings and restarts. `R` resets them to defaults. `S` requests status.
The board replies with JSON text frames. Example: `{"t":"status","usb":1,"caps":0,"os":"win","det":1,"ssid":"PicoKeys-AB12","def":0,"v":"2.1"}`

---

## Building from source

**Requirements**
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) **2.2.0**, with submodules (`git submodule update --init`)
- `arm-none-eabi-gcc` (tested with 14.2)
- CMake ≥ 3.13, Ninja (or Make), Python 3

**Build**
```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build -G Ninja        # PICO_BOARD defaults to pico_w
ninja -C build
# -> build/picokeys.uf2
```

On the first build the SDK downloads and builds `picotool` automatically (needs internet access to GitHub).

The web page (`web/index.html`) is gzipped and embedded automatically by `tools/embed_web.py`. Edit the HTML and rebuild.

---

## Project layout

```
PicoKeys/
├── CMakeLists.txt          Build definition (board: pico_w)
├── pico_sdk_import.cmake   Standard Pico SDK locator
├── web/
│   └── index.html          The phone keyboard/touchpad page
├── tools/
│   └── embed_web.py        Gzips the page into a C header at build time
├── src/
│   ├── main.c              Start-up, board detection, LED, BOOTSEL reset
│   ├── net.c / net.h       Common network interface, routes to the detected Wi-Fi chip
│   ├── cyw_net.c / .h      Genuine Pico W back end (CYW43439 + lwIP)
│   ├── esp_at.c / .h       Clone back end (ESP8285 AT-command driver)
│   ├── hal.h, hal_pico.c   UART0 (GP0/GP1) with interrupt-driven receive
│   ├── web_server.c / .h   HTTP, WebSocket, captive portal, DNS replies
│   ├── usb_hid.c / .h      HID report queues, keyboard LEDs, OS detection
│   ├── usb_descriptors.c   USB keyboard + mouse/media + CDC log port
│   ├── log.c / .h          "PicoKeys Log" serial port and its commands
│   ├── settings.c / .h     Hotspot name/password in flash
│   ├── sha1_b64.c / .h     SHA-1 + Base64 for the WebSocket handshake
│   ├── dhcpserver.c / .h   DHCP server for the Pico W hotspot (from pico-examples)
│   ├── lwipopts.h          lwIP configuration
│   └── tusb_config.h       TinyUSB configuration
└── test/
    └── sim_test.c          PC test against a simulated ESP8285
```

---

## Testing

`test/sim_test.c` runs the real ESP8285 driver and web server on a PC against a simulated ESP8285 that behaves like the old factory AT firmware. The simulator rejects `_CUR` commands, sends "busy" replies, and runs at different baud rates. The test checks:

- baud-rate detection and setup fallbacks
- the captive-portal redirect for the iPhone's connectivity check
- that the full page is delivered intact
- the WebSocket handshake and key presses
- saving a new password
- DNS answers
- releasing everything when the phone disconnects

Build the firmware once first, because the test uses the generated page header. Then run:

```bash
cd test
gcc -D_GNU_SOURCE -DPICOKEYS_HOST_TEST -O1 -g -fsanitize=address,undefined \
    -I../src -I../build/generated -DPICOKEYS_VERSION='"2.1"' \
    -o sim_test sim_test.c ../src/esp_at.c ../src/net.c ../src/web_server.c ../src/sha1_b64.c
./sim_test 115200        # optional argument: the simulated module's baud rate
```

---

## Credits and licenses

PicoKeys builds on:

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk): BSD-3-Clause
- [TinyUSB](https://github.com/hathach/tinyusb): MIT
- [lwIP](https://savannah.nongnu.org/projects/lwip/): BSD-style
- [cyw43-driver](https://github.com/georgerobotics/cyw43-driver): see its own license, which covers use with RP2040/RP2350
- `dhcpserver.c/.h` from [pico-examples](https://github.com/raspberrypi/pico-examples), originally from MicroPython: MIT (see `src/dhcpserver_LICENSE`)
- The OS-detection approach is adapted from [QMK](https://github.com/qmk/qmk_firmware)'s `os_detection` feature

<!-- Choose a license for PicoKeys itself (e.g. MIT) and add a LICENSE file. -->
