/* Copyright 2026 snackdriven
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * BLE / 2.4G support for the AL80's SmartBLE coprocessor.
 */

#include "quantum.h"
#include "al80.h"
#include "al80_logic.h"
#include <string.h>

#ifdef AL80_WIRELESS_ENABLE

#    include "host.h"
#    include "host_driver.h"
#    include "report.h"
#    include "usb_device_state.h"

/* ---- wire constants ---------------------------------------------------- */

#    define AL80_BLE_BAUD 460800
#    define AL80_BLE_SYNC 0x55

/* The module sleeps aggressively */
#    define AL80_BLE_WAKE_BYTES 60
#    define AL80_BLE_WAKE_SETTLE_MS 350

/* Advertised name. KB SD5 records the AL80 advertising as "YUNZII AL80 BT". */
#    define AL80_BLE_NAME "YUNZII AL80 BT"

/* START carries a fixed 20-byte payload (name padded with zeroes). */
#    define AL80_BLE_START_LEN 20

/* Inter-report pacing */
#    define AL80_BLE_REPORT_GAP_MS 8
#    define AL80_24G_REPORT_GAP_MS 2

/* ---- state ------------------------------------------------------------- */

static const SerialConfig ble_serial_config = {AL80_BLE_BAUD, 0, 0, 0};

static bool          wireless_started   = false; /* SD1 up */
static uint8_t       wl_led_state       = 0;     /* host lock LEDs as relayed by the module */
static bool          wl_host_suspended  = false; /* 2.4G host went to sleep */
static uint32_t      wl_last_activity   = 0;     /* last report handed to the module */
static bool          wireless_connected = false; /* module reports a live link */
static al80_wl_mode_t kb_mode           = AL80_WL_USB;
static host_driver_t *usb_driver        = NULL;
static uint32_t       last_report_time  = 0;

/* Set by process_record_kb, consumed by al80_wireless_task */
static uint8_t g_wireless_request = 0;
#    define AL80_REQ_PAIR_FLAG 0x80
#    define AL80_REQ_NOSAVE_FLAG 0x40 /* restore/fallback: don't rewrite the boot mode */

/* Diagnostics */
static uint16_t dbg_tx_bytes = 0;
static uint16_t dbg_rx_bytes = 0;
static uint8_t  dbg_last_rx[4] = {0, 0, 0, 0};

/* Ring of the last 6 COMPLETE frames (cmd, mode, data) */
#    define AL80_WL_FRAME_LOG 6
static uint8_t dbg_frames[AL80_WL_FRAME_LOG][3];
static uint8_t dbg_frame_head  = 0;
static uint8_t dbg_frame_count = 0;

/* If a mode switch never produces a connection, fall back to USB rather than leaving the keyboard mute */
#    ifndef AL80_WL_CONNECT_TIMEOUT_MS
#        define AL80_WL_CONNECT_TIMEOUT_MS 6000
#    endif
static uint32_t wl_switch_time = 0;

/* Optimistic mode */
#    ifndef AL80_WL_OPTIMISTIC
#        define AL80_WL_OPTIMISTIC 1
#    endif

/* ---- boot mode --------------------------------------------------------- */

/* Coming up in USB every time means pressing Fn+Q after every power-on -- and on battery there is no */
#    define AL80_WL_BOOT_SHIFT 8
#    define AL80_WL_BOOT_MASK 0x0000FF00u
#    define AL80_WL_BOOT_TAG 0xA0

/* How long USB gets to enumerate before we conclude there is no host and go wireless */
#    ifndef AL80_WL_BOOT_GRACE_MS
#        define AL80_WL_BOOT_GRACE_MS 2500
#    endif

static bool     wl_boot_done = false; /* boot decision made */
static uint32_t wl_boot_time = 0;     /* set in init: start of the grace window */
static bool     wl_auto_mode = false; /* mode came from a restore, not a keypress */

/* Mode field of the last frame the module sent (1..4) */
static uint8_t module_mode = 0;

/* ---- mode switch ------------------------------------------------------- */

/* The BT / wired / dongle slide switch pulls one of two MCU pins low */
#    define AL80_SW_BT_PIN C14
#    define AL80_SW_24G_PIN C15

static al80_switch_t sw_pos  = AL80_SW_WIRED; /* debounced position */
static bool          sw_seen = false;         /* a wireless position was read at least once */

static al80_switch_t sw_read_raw(void) {
    return al80_switch_decode(readPin(AL80_SW_BT_PIN), readPin(AL80_SW_24G_PIN));
}

/* ---- low level --------------------------------------------------------- */

/* Count what the driver ACCEPTED, not what we asked it to send */
static inline void ble_put(uint8_t b) {
    dbg_tx_bytes += (uint16_t)sdWrite(&SD1, &b, 1);
}

static void ble_write(const uint8_t *buf, size_t len) {
    dbg_tx_bytes += (uint16_t)sdWrite(&SD1, buf, len);
}

/* 60 zero bytes + settle */
static void ble_wake(uint16_t settle_ms) {
    static const uint8_t zeros[AL80_BLE_WAKE_BYTES] = {0};
    ble_write(zeros, sizeof(zeros));
    wait_ms(settle_ms);
}

/* ---- commands ---------------------------------------------------------- */

/* PAIR: enter pairing/discoverable for `mode`. Sent twice -- the module drops the first frame often */
static void ble_cmd_pair(uint8_t mode) {
    const uint8_t pkt[5] = {AL80_BLE_SYNC, 0x03, 0x00, mode, 0x01};
    for (uint8_t i = 0; i < 2; i++) {
        ble_write(pkt, sizeof(pkt));
        wait_ms(10);
    }
}

/* START: connect in `mode`, advertising as AL80_BLE_NAME. Payload is fixed length and zero padded */
/* 22 bytes total */
static void ble_cmd_start(uint8_t mode) {
    uint8_t pkt[2 + AL80_BLE_START_LEN];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = AL80_BLE_SYNC;
    pkt[1] = AL80_BLE_START_LEN;   /* 0x14 = 20 bytes follow */
    pkt[2] = 0x00;                 /* command */
    pkt[3] = mode;
    memcpy(&pkt[4], AL80_BLE_NAME, sizeof(AL80_BLE_NAME) - 1); /* 14 chars, no NUL */
    pkt[18] = (uint8_t)('0' + mode);
    /* pkt[19..21] stay zero */
    for (uint8_t i = 0; i < 2; i++) {
        ble_write(pkt, sizeof(pkt));
        wait_ms(10);
    }
}

static void ble_cmd_stop(void) {
    const uint8_t pkt[4] = {AL80_BLE_SYNC, 0x02, 0x00, 0x00};
    ble_write(pkt, sizeof(pkt));
}

/* Should we hand this report to the module? In optimistic mode, being in a wireless mode is enough */
static inline bool wl_should_send(void) {
#    if AL80_WL_OPTIMISTIC
    return kb_mode != AL80_WL_USB;
#    else
    return wireless_connected;
#    endif
}

/* Battery percentage for the host's battery service */
void al80_wireless_battery_push(uint8_t pct) {
    if (!wireless_started || kb_mode == AL80_WL_USB || kb_mode == AL80_WL_24G || !wireless_connected) return;
    const uint8_t pkt[4] = {AL80_BLE_SYNC, 0x02, 0x09, pct};
    ble_write(pkt, sizeof(pkt));
}

/* ---- host driver ------------------------------------------------------- */

/* The module owns the lock LEDs while wireless; it pushes state back to us via the inbound parser */
static uint8_t wl_keyboard_leds(void) {
    return wl_led_state;
}

/* Pace reports so we never outrun the module */
static void wl_pace(void) {
    const uint16_t gap     = (kb_mode == AL80_WL_24G) ? AL80_24G_REPORT_GAP_MS : AL80_BLE_REPORT_GAP_MS;
    const uint32_t elapsed = timer_elapsed32(last_report_time);
    if (elapsed < gap) {
        wait_ms(gap - elapsed);
    }
    last_report_time = timer_read32();
}

static void wl_send_keyboard(report_keyboard_t *report) {
    if (!wl_should_send()) return;
    ble_put(AL80_BLE_SYNC);
    ble_put(0x09);
    ble_put(0x01);
    ble_write((uint8_t *)report, KEYBOARD_REPORT_SIZE);
    wl_last_activity = timer_read32();
    wl_pace();
}

#    ifdef NKRO_ENABLE
static void wl_send_nkro(report_nkro_t *report) {
    if (!wl_should_send()) return;
    ble_put(AL80_BLE_SYNC);
    ble_put(0x15);
    ble_write((uint8_t *)report, 0x15);
    wl_pace();
}
#    else
#        define wl_send_nkro NULL
#    endif

/* Every report is `55 <len> <report id> <body>`: the module forwards by report id. */
static void wl_send_mouse(report_mouse_t *report) {
#    ifdef MOUSE_SHARED_EP
    if (!wl_should_send() || sizeof(report_mouse_t) != 6) return;
    ble_put(AL80_BLE_SYNC);
    ble_put(sizeof(report_mouse_t));
    ble_write((uint8_t *)report, sizeof(report_mouse_t));
    wl_last_activity = timer_read32();
    wl_pace();
#    else
    (void)report;
#    endif
}

/* Consumer (media) and system reports: id 3 = system, id 4 = consumer, then a 16-bit usage. */
static void wl_send_extra(report_extra_t *report) {
    if (!wl_should_send()) return;
    ble_put(AL80_BLE_SYNC);
    ble_put(sizeof(report_extra_t));
    ble_write((uint8_t *)report, sizeof(report_extra_t));
    wl_last_activity = timer_read32();
    wl_pace();
}

#    ifdef RAW_ENABLE
/* Raw HID is the LCD/control channel and is USB-only by design */
static void wl_send_raw_hid(uint8_t *data, uint8_t length) {
    /* The module does not carry vendor traffic, but the cable may still be in */
    if (usb_driver && usb_driver->send_raw_hid &&
        usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED) {
        usb_driver->send_raw_hid(data, length);
    }
}
#    endif

static host_driver_t wl_driver = {
    wl_keyboard_leds, wl_send_keyboard, wl_send_nkro, wl_send_mouse, wl_send_extra,
#    ifdef RAW_ENABLE
    wl_send_raw_hid,
#    endif
};

/* ---- inbound ----------------------------------------------------------- */

/* Module -> STM32 frames are `55 03 <cmd> <mode> <data>`. Only three commands exist (KB SD5) */
static void ble_handle_frame(uint8_t cmd, uint8_t mode, uint8_t data) {
    dbg_frames[dbg_frame_head][0] = cmd;
    dbg_frames[dbg_frame_head][1] = mode;
    dbg_frames[dbg_frame_head][2] = data;
    dbg_frame_head = (uint8_t)((dbg_frame_head + 1) % AL80_WL_FRAME_LOG);
    if (dbg_frame_count < AL80_WL_FRAME_LOG) dbg_frame_count++;

    if (mode >= AL80_WL_BT1 && mode <= AL80_WL_24G) module_mode = mode;

    /* Frames describe one mode; ignore the ones that are not about the mode we are in. */
    if (mode != (uint8_t)kb_mode) return;

    switch (cmd) {
        case 0x00: /* connection status: 0 = link up (the vendor firmware tests data == 0) */
            wireless_connected = al80_wl_link_up(data);
            break;
        case 0x01: /* host lock LED state, returned through the host driver like USB does */
            wl_led_state = data;
            break;
        case 0x02: /* 2.4G host power: 0xAA suspend, 0xBB resume */
            if (data == 0xAA) {
                wl_host_suspended = true;
            } else if (data == 0xBB) {
                wl_host_suspended = false;
            }
            break;
        default:
            break;
    }
}

/* Non-blocking drain of SD1 */
static void ble_poll_rx(void) {
    static al80_wl_parser_t parser;
    msg_t                   c;

    while ((c = sdGetTimeout(&SD1, TIME_IMMEDIATE)) >= 0) {
        uint8_t b = (uint8_t)c;
        dbg_rx_bytes++;
        dbg_last_rx[0] = dbg_last_rx[1];
        dbg_last_rx[1] = dbg_last_rx[2];
        dbg_last_rx[2] = dbg_last_rx[3];
        dbg_last_rx[3] = b;
        if (al80_wl_parse(&parser, b)) {
            ble_handle_frame(parser.buf[2], parser.buf[3], parser.buf[4]);
        }
    }
}

/* ---- public API -------------------------------------------------------- */

/* ---- boot mode storage ------------------------------------------------- */

static al80_wl_mode_t wl_boot_mode_load(void) {
    const uint8_t b = (uint8_t)((eeconfig_read_user() & AL80_WL_BOOT_MASK) >> AL80_WL_BOOT_SHIFT);
    if ((b & 0xF0) != AL80_WL_BOOT_TAG) return AL80_WL_USB; /* never written */
    const uint8_t m = b & 0x0F;
    return (m <= (uint8_t)AL80_WL_24G) ? (al80_wl_mode_t)m : AL80_WL_USB;
}

static void wl_boot_mode_save(al80_wl_mode_t mode) {
    const uint32_t ecu  = eeconfig_read_user();
    const uint32_t want = (ecu & ~AL80_WL_BOOT_MASK) |
                          ((uint32_t)(AL80_WL_BOOT_TAG | (uint8_t)mode) << AL80_WL_BOOT_SHIFT);
    /* Emulated flash here is wear-levelled but not free, and a mode you are already booting into is not */
    if (want != ecu) eeconfig_update_user(want);
}

void al80_wireless_init(void) {
    /* USART1 on its default pins */
    setPinInputHigh(AL80_SW_BT_PIN);
    setPinInputHigh(AL80_SW_24G_PIN);
    palSetPadMode(GPIOA, 9, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
    palSetPadMode(GPIOA, 10, PAL_MODE_INPUT);
    sdStart(&SD1, &ble_serial_config);
    wireless_started = true;
    usb_driver       = host_get_driver();
    wl_boot_time     = timer_read32();
}

al80_wl_mode_t al80_wireless_mode(void) {
    return kb_mode;
}

bool al80_wireless_is_connected(void) {
    return wireless_connected;
}

/* Snapshot for the raw-HID diagnostic (0x4C) */
void al80_wireless_debug(uint8_t *out) {
    out[0] = (uint8_t)kb_mode;
    out[1] = wireless_connected ? 1 : 0;
    out[2] = (uint8_t)(dbg_tx_bytes >> 8);
    out[3] = (uint8_t)(dbg_tx_bytes & 0xFF);
    out[4] = (uint8_t)(dbg_rx_bytes >> 8);
    out[5] = (uint8_t)(dbg_rx_bytes & 0xFF);
    out[6] = dbg_last_rx[0];
    out[7] = dbg_last_rx[1];
    out[8] = dbg_last_rx[2];
    out[9] = dbg_last_rx[3];

    out[10] = (uint8_t)SD1.state;                       /* 3 = SD_READY */
    const uint16_t brr = (uint16_t)USART1->BRR;
    const uint16_t cr1 = (uint16_t)USART1->CR1;
    const uint16_t cr2 = (uint16_t)USART1->CR2;
    const uint16_t cr3 = (uint16_t)USART1->CR3;
    const uint16_t sr  = (uint16_t)USART1->SR;
    out[11] = (uint8_t)(brr >> 8);  out[12] = (uint8_t)brr;
    out[13] = (uint8_t)(cr1 >> 8);  out[14] = (uint8_t)cr1;
    out[15] = (uint8_t)(cr2 >> 8);  out[16] = (uint8_t)cr2;
    out[17] = (uint8_t)(cr3 >> 8);  out[18] = (uint8_t)cr3;
    out[19] = (uint8_t)(sr >> 8);   out[20] = (uint8_t)sr;

    const uint32_t crh = GPIOA->CRH;                    /* PA8..PA15 */
    out[21] = (uint8_t)(crh >> 24); out[22] = (uint8_t)(crh >> 16);
    out[23] = (uint8_t)(crh >> 8);  out[24] = (uint8_t)crh;

    /* frame ring: count, head, then 6 x (cmd, mode, data) */
    out[25] = dbg_frame_count;
    out[26] = dbg_frame_head;
    for (uint8_t i = 0; i < AL80_WL_FRAME_LOG; i++) {
        out[27 + i * 3 + 0] = dbg_frames[i][0];
        out[27 + i * 3 + 1] = dbg_frames[i][1];
        out[27 + i * 3 + 2] = dbg_frames[i][2];
    }

    /* boot-mode state */
    out[45] = (uint8_t)((eeconfig_read_user() & AL80_WL_BOOT_MASK) >> AL80_WL_BOOT_SHIFT);
    out[46] = module_mode;
    out[47] = (uint8_t)((wl_boot_done ? 0x01 : 0) | (wl_auto_mode ? 0x02 : 0));

    out[48] = (uint8_t)((readPin(AL80_SW_BT_PIN) ? 0x01 : 0) | (readPin(AL80_SW_24G_PIN) ? 0x02 : 0) |
                        (sw_seen ? 0x04 : 0) | (wl_host_suspended ? 0x08 : 0));
    out[49] = (uint8_t)sw_pos;
    out[50] = wl_led_state;
}

bool al80_wireless_host_suspended(void) {
    return wl_host_suspended;
}

uint32_t al80_wireless_last_activity(void) {
    return wl_last_activity;
}

/* Called from process_record_kb */
static void wl_request(al80_wl_mode_t mode, bool pair, bool save) {
    g_wireless_request = (uint8_t)(mode + 1) | (pair ? AL80_REQ_PAIR_FLAG : 0) |
                         (save ? 0 : AL80_REQ_NOSAVE_FLAG);
}

void al80_wireless_request(al80_wl_mode_t mode, bool pair) {
    /* A keypress is the user choosing, and that is what gets remembered. */
    wl_request(mode, pair, true);
}

static void al80_wireless_apply(al80_wl_mode_t mode, bool pair, bool save) {
    if (!wireless_started) return;

    if (save) wl_boot_mode_save(mode);

    if (mode == AL80_WL_USB) {
        ble_wake(100);
        ble_cmd_stop();
        wireless_connected = false;
        kb_mode            = AL80_WL_USB;
        wl_auto_mode       = false;
        if (usb_driver) host_set_driver(usb_driver);
        return;
    }

    /* Module modes are 1-based: 1/2/3 = BLE slots, 4 = 2.4G dongle. */
    const uint8_t wire_mode = (uint8_t)mode;

    wireless_connected = false;
    wl_led_state       = 0;
    wl_host_suspended  = false;
    wl_last_activity   = timer_read32();
    ble_wake(AL80_BLE_WAKE_SETTLE_MS);
    if (pair) {
        ble_cmd_pair(wire_mode);
    } else {
        ble_cmd_start(wire_mode);
    }

    kb_mode        = mode;
    wl_auto_mode   = !save; /* restored rather than asked for: USB may reclaim us */
    wl_switch_time = timer_read32();
    if (!usb_driver) usb_driver = host_get_driver();
    host_set_driver(&wl_driver);
}

/* Driven from housekeeping_task_kb */
void al80_wireless_task(bool screen_busy) {
    if (!wireless_started) return;

    ble_poll_rx();

    /* Debounce the mode switch: a position must hold for 50ms to count. */
    {
        static al80_switch_t sw_last    = AL80_SW_WIRED;
        static uint32_t      sw_changed = 0;
        const al80_switch_t  now        = sw_read_raw();
        if (now != sw_last) {
            sw_last    = now;
            sw_changed = timer_read32();
        } else if (now != sw_pos && now != AL80_SW_INVALID && timer_elapsed32(sw_changed) > 50) {
            sw_pos = now;
            if (now != AL80_SW_WIRED) sw_seen = true;
        }
    }

    /* Failsafe */
#    if !AL80_WL_OPTIMISTIC
    if (kb_mode != AL80_WL_USB && !wireless_connected && wl_switch_time &&
        timer_elapsed32(wl_switch_time) > AL80_WL_CONNECT_TIMEOUT_MS) {
        wl_switch_time = 0;
        kb_mode        = AL80_WL_USB;
        if (usb_driver) host_set_driver(usb_driver);
        clear_keyboard();
    }
#    endif

    /* Boot decision, made once */
    if (!wl_boot_done) {
        if (usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED) {
            wl_boot_done = true; /* a host owns us; USB stays */
        } else if (timer_elapsed32(wl_boot_time) > AL80_WL_BOOT_GRACE_MS) {
            wl_boot_done              = true;
            al80_wl_mode_t want = module_mode ? (al80_wl_mode_t)module_mode : wl_boot_mode_load();
            if (sw_pos == AL80_SW_24G) {
                want = AL80_WL_24G;
            } else if (sw_pos == AL80_SW_BT && (want < AL80_WL_BT1 || want > AL80_WL_BT3)) {
                const al80_wl_mode_t stored = wl_boot_mode_load();
                want = (stored >= AL80_WL_BT1 && stored <= AL80_WL_BT3) ? stored : AL80_WL_BT1;
            }
            if (want != AL80_WL_USB) wl_request(want, false, false);
        }
    }

    /* Cable came back while we were on a mode nobody asked for */
    if (wl_auto_mode && kb_mode != AL80_WL_USB &&
        usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED) {
        wl_auto_mode = false;
        clear_keyboard();
        wl_request(AL80_WL_USB, false, false);
    }

    if (g_wireless_request && !screen_busy) {
        const uint8_t req  = g_wireless_request;
        const uint8_t mode = (uint8_t)((req & 0x0F) - 1);
        g_wireless_request = 0;
        al80_wireless_apply((al80_wl_mode_t)mode, (req & AL80_REQ_PAIR_FLAG) != 0,
                            (req & AL80_REQ_NOSAVE_FLAG) == 0);
    }
}

#endif /* AL80_WIRELESS_ENABLE */
