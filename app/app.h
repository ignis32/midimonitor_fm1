/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 ignis32 (from fm1-base's template, Copyright (C) 2026 Charles Vestal) */
/* MIDIMON: the app's half of the contract with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "MIDIMON"
#define APP_SPLASH_RGB RGB(110, 255, 150)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_CANVAS_PX (240u * 192u)      /* the log, whole: 16 lines */
#define APP_BTDEV                        /* Bluetooth MIDI devices (lib/btdev.h) */
#define APP_LOOP                         /* app_loop: the tap drained every pass of the main loop */
#define APP_BLE_AT_BOOT (!mon_ble_off())  /* Bluetooth at power-on: as the player left it (REC held) */
#define FM1B_USB_HOST 1                  /* the USB-C port as a USB MIDI host (GLO K4; the usb host patches) */
int mon_ble_off(void);

/* the store: the app's objects, the panel calibration (the platform's) and Bluetooth's address and pairing among
 * them, and their flash offsets. All in the data area, which stock V15 wipes: the settings sectors at 0xFC000
 * (which V15 keeps) hold another firmware's settings, and MIDIMON leaves them alone */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE, OBJ_NOBJ };
#define APP_STORE_MAP {0xDA000u, 0xDC000u, 0xDE000u}

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_loop(void);                     /* every pass of the main loop: the tap into the log */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* the host simulator's checks (host/plat_host.c "expect"; app.c app_expect has the list) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
