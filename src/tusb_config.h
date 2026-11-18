#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

#define CFG_TUD_ENABLED       1
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__ ((aligned(4)))
#endif

#define CFG_TUD_ENDPOINT0_SIZE    64

// Two HID interfaces: 0 = boot keyboard, 1 = mouse + consumer (media) keys
#define CFG_TUD_HID               2
#define CFG_TUD_CDC               1   // "PicoKeys log" serial port
#define CFG_TUD_MSC               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0

#define CFG_TUD_HID_EP_BUFSIZE    16
#define CFG_TUD_CDC_RX_BUFSIZE    256
#define CFG_TUD_CDC_TX_BUFSIZE    1024
#define CFG_TUD_CDC_EP_BUFSIZE    64

#endif
