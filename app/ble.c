/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 ignis32 */
/* Bluetooth (GLO 2/3), both roles at once:
 *   BLE-PERIPHERY  a computer finds the FM-1 (FM1_MONITOR) and connects to it (on / off: plat_ble_advertise)
 *   BLE-CENTRAL    the FM-1 connects to a Bluetooth MIDI device: OFF; FIRST, the first one the scan sees (again
 *                  when it goes); CHOOSE, the one chosen in the device list, and with RECONNECT on, that one again
 *                  at power-on and whenever it goes (found by its address)
 * The radio is off only when both are. The device list is a pop-up over the page: the scan's devices with the last
 * four digits of their address, SELECT or K3 moves, OCT+ connects, OCT- closes, PLAY looks again (the list
 * starts empty). Part of app.c, after views.c. */

static void changed(void);

static int ble_radio_wanted(void) { return set.ble_p || set.ble_c != BLEC_OFF; }
int mon_ble_on(void) { return ble_radio_wanted(); }   /* (app.h APP_BLE_AT_BOOT) */

/* the device the scan found at i: connect to it */
static void ble_connect_to(int i)
{
    if (!plat_ble_found(i, ui.cdev_name, 0) || !plat_ble_found_addr(i, ui.cdev_addr))
        return;
    if (plat_ble_connect(i) == 0)
        app_say("BLE-C CONNECTING", ui.cdev_name);
    else
        ui.ble_wait = plat_ms() + 3000u;
}

/* the settings into the radio: at boot (the radio not up yet: it starts as APP_BLE_AT_BOOT says) and after a change */
static void ble_apply(int boot)
{
    plat_ble_advertise(set.ble_p);
    if (set.ble_c == BLEC_OFF) {
        plat_ble_scan(0);
        if (plat_ble_device() != BLE_DEV_OFF)
            plat_ble_disconnect();
        ui.ble_try = 0;
        ui.picker = 0;
    }
    if (boot) {
        ui.ble_try = set.ble_c == BLEC_CHOOSE && set.ble_auto && set.ble_have;
        return;
    }
    if (ble_radio_wanted() != (plat_ble_state() != BLE_OFF))
        plat_ble_enable(ble_radio_wanted());
}

/* every frame: what happened (in words), and BLE-CENTRAL's scanning and connecting */
static void ble_task(void)
{
    uint32_t now = plat_ms();
    int dev = plat_ble_device(), pc = plat_ble_connected(), i;
    if (pc != ui.ble_pc) {
        ui.ble_pc = (uint8_t)pc;
        app_say("BLE-P", pc ? "COMPUTER CONNECTED" : "COMPUTER GONE");
    }
    if (dev != ui.ble_dev) {
        if (dev == BLE_DEV_READY) {
            app_say("BLE-C CONNECTED", ui.cdev_name);
            ui.ble_try = 0;
        } else if (ui.ble_dev == BLE_DEV_READY) {   /* it went: FIRST looks again; CHOOSE does if RECONNECT is on (or
                                                     * another one was just chosen) */
            app_say("BLE-C", "LOST");
            ui.ble_try = ui.ble_try || (set.ble_c == BLEC_CHOOSE && set.ble_auto && set.ble_have);
            ui.ble_wait = now + 1000u;
        } else if ((ui.ble_dev == BLE_DEV_CONNECTING || ui.ble_dev == BLE_DEV_SEARCHING) && dev == BLE_DEV_OFF) {
            app_say("BLE-C", "NO ANSWER");
            ui.ble_wait = now + 3000u;
        }
        ui.ble_dev = dev;
        ui.redraw_all = 1;
    }
    if (plat_ble_state() != BLE_READY || set.ble_c == BLEC_OFF)
        return;
    if (dev == BLE_DEV_CONNECTING || dev == BLE_DEV_SEARCHING || dev == BLE_DEV_READY)
        return;
    if (!ui.picker && set.ble_c != BLEC_FIRST && !ui.ble_try) {   /* nothing to look for */
        if (dev == BLE_DEV_SCANNING)
            plat_ble_scan(0);
        return;
    }
    if (dev == BLE_DEV_OFF) {
        plat_ble_scan(1);
        return;
    }
    if (ui.picker || (int32_t)(now - ui.ble_wait) < 0)
        return;                        /* (the list is the player's; after a failure, a pause) */
    if (set.ble_c == BLEC_FIRST) {
        if (plat_ble_found(0, 0, 0))
            ble_connect_to(0);
        return;
    }
    for (i = 0; i < 8; i++) {          /* CHOOSE: the chosen one, by its address */
        uint8_t a[6];
        if (!plat_ble_found_addr(i, a))
            break;
        if (!memcmp(a, set.ble_addr, 6)) {
            ble_connect_to(i);
            return;
        }
    }
}

/* ---- the page's knobs (GLO 2/3) */
static int ble_count(void)
{
    int n = 0;
    while (n < 8 && plat_ble_found(n, 0, 0))
        n++;
    return n;
}

static void picker_move(int32_t e)
{
    int n = ble_count(), p = (int)ui.pick + (int)e;
    p = n ? (p < 0 ? 0 : p >= n ? n - 1 : p) : 0;
    ui.pick = (uint8_t)p;
    if (ui.pick < ui.pick_top)
        ui.pick_top = ui.pick;
    if (ui.pick >= ui.pick_top + PICK_ROWS)
        ui.pick_top = (uint8_t)(ui.pick - PICK_ROWS + 1);
    ui.redraw_all = 1;
}

static void ble_knob(int k, int32_t e)   /* k: 1..4 */
{
    if (k == 1) {
        set.ble_p = (uint8_t)!set.ble_p;
        changed();
        ble_apply(0);
        app_say("BLE-PERIPHERY", set.ble_p ? "ON" : "OFF");
    } else if (k == 2) {
        set.ble_c = (uint8_t)((set.ble_c + (e > 0 ? 1u : 2u)) % 3u);
        changed();
        ble_apply(0);
        ui.ble_try = set.ble_c == BLEC_CHOOSE && set.ble_auto && set.ble_have;
        app_say("BLE-CENTRAL", BLEC_NAME[set.ble_c]);
    } else if (k == 3) {
        if (ui.picker) {
            picker_move(e);
        } else if (!FM1B_BLE_CENTRAL || set.ble_c != BLEC_CHOOSE) {
            app_say("BLE-CENTRAL", "TO CHOOSE: SET IT TO CHOOSE");
        } else {
            ui.picker = 1;             /* (the scan starts: ble_task) */
            ui.pick = ui.pick_top = 0;
            ui.redraw_all = 1;
        }
    } else if (k == 4) {
        set.ble_auto = (uint8_t)!set.ble_auto;
        changed();
        app_say("RECONNECT", set.ble_auto ? "ON" : "OFF");
    }
}

/* a button while the device list is up: 1 = taken */
static int picker_button(uint32_t k)
{
    uint8_t a[6];
    if (!ui.picker || k == B_HOME)       /* (HOME goes home: the list closes as its page goes) */
        return 0;
    if (k == B_OCTDN) {
        ui.picker = 0;
    } else if (k == B_PLAY) {          /* look again: the list starts empty */
        if (plat_ble_device() == BLE_DEV_SCANNING)
            plat_ble_scan(0);
        ui.pick = ui.pick_top = 0;
    } else if (k == B_OCTUP && plat_ble_found(ui.pick, set.ble_name, 0) && plat_ble_found_addr(ui.pick, a)) {
        memcpy(set.ble_addr, a, 6);    /* the chosen one, kept at once (reconnecting after power-on needs it) */
        set.ble_have = 1;
        plat_store_save(OBJ_SETTINGS, &set, sizeof set);
        ui.picker = 0;
        ui.ble_try = 1;
        ui.ble_wait = 0;
        if (plat_ble_device() != BLE_DEV_OFF && plat_ble_device() != BLE_DEV_SCANNING)
            plat_ble_disconnect();     /* (another one connected: let it go; the new one is found by its address) */
        app_say("BLE-C CHOSEN", set.ble_name);
    }
    ui.redraw_all = 1;
    return 1;
}
