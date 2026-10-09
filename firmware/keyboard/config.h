/* Copyright 2026 snackdriven
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * YUNZII AL80 (STM32F103xB, stm32duino bootloader) Hardware params recovered by RIPPLE.bin.
 */
#pragma once

/* ---- aw20216s RGB matrix driver on SPI1 (no remap) ---- */
/* SPI1: SCK=A5, MISO=A6, MOSI=A7 (STM32F103 default AF, mode 0, MSB-first) */
#define SPI_DRIVER   SPID1
#define SPI_SCK_PIN  A5
#define SPI_MISO_PIN A6
#define SPI_MOSI_PIN A7

/* aw20216s chip-selects */
#define AW20216S_CS_PIN_1 B6   /* driver 0 */
#define AW20216S_CS_PIN_2 C8   /* driver 1 */
/* aw20216s hardware enable */
#define AW20216S_EN_PIN B7

/* SPI mode 0, /4 divisor are the driver defaults; keep explicit for clarity */
#define AW20216S_SPI_MODE    0
#define AW20216S_SPI_DIVISOR 4

/* Per-driver LED split from the cert source (sum must equal LED count = 84) */
#define DRIVER_1_LED_TOTAL 45
#define DRIVER_2_LED_TOTAL 39

/* ---- firmware identity and behaviour ---- */
/* Build counter reported by the 0x4C diagnostic, bumped on every flashed build. */
#define AL80_FW_BUILD 8
/* On battery, lights and LCD go dark after this long without a key or knob event. */
#ifndef AL80_IDLE_MS
#    define AL80_IDLE_MS 300000
#endif

/* LCD rotation: how long the home page and the GIF page each stay up. */
#ifndef AL80_ROTATE_HOME_MS
#    define AL80_ROTATE_HOME_MS 20000
#endif
#ifndef AL80_ROTATE_GIF_MS
#    define AL80_ROTATE_GIF_MS 10000
#endif

/* ---- dynamic keymap ---- */
/* Vial UID + unlock combo live in keymaps/vial/config.h */
#define DYNAMIC_KEYMAP_LAYER_COUNT 4

/* ---- LCD pass-through over USART3 (partial remap TX=PC10, RX=PC11) ---- */
/* Baud 460800, 8N1, interrupt-driven (no DMA). SD3 started in post_init. */
#define AL80_LCD_BAUD 921600

/* raw-HID endpoint/report size */
#define RAW_EPSIZE 64

/* raw-HID report ids used by the LCD pass-through protocol */
#define AP_W_SCREEN_INFO 0x40
#define AP_W_SCREEN_DATA 0x41
#define AP_GIVE_SCREEN_SEM 0x42

/* user-editable RGB palette (PALETTE_CYCLE effect) ---- A RAM mirror of PALETTE_LEN {hue,sat} pairs */
#define AL80_PALETTE_LEN 3

/* Dedicated KB datablock */
#define AL80_PALETTE_STORE_SIZE (1 + AL80_PALETTE_LEN * 2)
#define AL80_BAR_STORE_SIZE     5   /* magic + h + s + v + independent */
/* Backlight theme for the GIF page: magic + scene count + AL80_THEME_MAX x {tenths of a second, hue, sat}. */
#define AL80_THEME_MAX          8
#define AL80_THEME_STORE_SIZE   (2 + AL80_THEME_MAX * 3)
#define EECONFIG_KB_DATA_SIZE   (AL80_PALETTE_STORE_SIZE + AL80_BAR_STORE_SIZE + AL80_THEME_STORE_SIZE)

/* raw-HID palette protocol opcodes (top-level, alongside LCD 0x40..0x42) */
#define AP_PALETTE_GET  0x43
#define AP_PALETTE_SET  0x44
#define AP_PALETTE_SAVE 0x45

/* raw-HID side-LED-bar protocol opcodes (mirror the palette ones) */
#define AP_BAR_GET  0x46
#define AP_BAR_SET  0x47
#define AP_BAR_SAVE 0x48

/* raw-HID per-key live LED stream (host -> keyboard) */
#define AP_LIVE_LEDS 0x49

/* raw-HID backlight theme for the GIF page */
#define AP_THEME_SET 0x50
#define AP_THEME_GET 0x51

/* keyboard -> host panel-request signal (UNSOLICITED raw_hid_send, not a reply) */
#define AP_PANEL_REQ 0x4B

/* Live-LED stream idle fallback + brightness cap (defense in depth; the host also caps) */
#ifndef AL80_LIVE_IDLE_MS
#    define AL80_LIVE_IDLE_MS 500
#endif
#ifndef AL80_LIVE_MAX_VAL
#    define AL80_LIVE_MAX_VAL 160   /* ~63% cap (NFR1). Set 255 to disable the firmware cap. */
#endif

/* VialRGB effect id for the custom PALETTE_CYCLE effect */
#define AL80_VIALRGB_PALETTE_CYCLE_ID 0x0100

/* built-in RGB matrix effects (v18 */
#define ENABLE_RGB_MATRIX_BREATHING
#define ENABLE_RGB_MATRIX_BAND_VAL
#define ENABLE_RGB_MATRIX_CYCLE_ALL
#define ENABLE_RGB_MATRIX_CYCLE_LEFT_RIGHT
#define ENABLE_RGB_MATRIX_CYCLE_UP_DOWN
#define ENABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON
#define ENABLE_RGB_MATRIX_CYCLE_OUT_IN
#define ENABLE_RGB_MATRIX_CYCLE_PINWHEEL
#define ENABLE_RGB_MATRIX_CYCLE_SPIRAL
#define ENABLE_RGB_MATRIX_DUAL_BEACON
#define ENABLE_RGB_MATRIX_RAINBOW_BEACON
#define ENABLE_RGB_MATRIX_RAINDROPS
#define ENABLE_RGB_MATRIX_HUE_BREATHING
#define ENABLE_RGB_MATRIX_HUE_WAVE
/* framebuffer effects (need a per-frame buffer; ~90 bytes on this 6x15 matrix) */
#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define ENABLE_RGB_MATRIX_DIGITAL_RAIN
#define ENABLE_RGB_MATRIX_PIXEL_RAIN

/* v20 */
#define RGB_MATRIX_KEYREACTIVE_ENABLED
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_SIMPLE
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_WIDE
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_CROSS
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTICROSS
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_NEXUS
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS
#define ENABLE_RGB_MATRIX_SPLASH
#define ENABLE_RGB_MATRIX_MULTISPLASH
#define ENABLE_RGB_MATRIX_SOLID_SPLASH
#define ENABLE_RGB_MATRIX_SOLID_MULTISPLASH
