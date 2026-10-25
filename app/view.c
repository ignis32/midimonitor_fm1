/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 ignis32 */
/* The screens, in a green phosphor terminal's look: the tabs (header), the page (body: LOG, FILTER, STATS, THRU, GLO;
 * the LOG tab's other views in views.c) and the sources (footer). Part of app.c, after mon.c. */

/* the phosphor's shades, per skin (GLO 2, K1): background, faint, dim, mid, lit, glow */
static const uint16_t SKINS[][6] = {
    {RGB(2, 12, 6), RGB(14, 52, 28), RGB(30, 104, 56), RGB(56, 184, 96), RGB(112, 255, 150), RGB(196, 255, 214)},
    {RGB(14, 8, 0), RGB(60, 34, 0), RGB(122, 72, 0), RGB(204, 124, 12), RGB(255, 178, 44), RGB(255, 222, 150)},
    {RGB(0, 8, 14), RGB(0, 42, 62), RGB(0, 92, 124), RGB(40, 172, 212), RGB(124, 230, 255), RGB(214, 246, 255)},
    {RGB(8, 8, 8), RGB(50, 50, 50), RGB(100, 100, 100), RGB(170, 170, 170), RGB(236, 236, 236), RGB(255, 255, 255)},
    {RGB(12, 0, 0), RGB(62, 10, 10), RGB(124, 26, 24), RGB(204, 54, 42), RGB(255, 104, 84), RGB(255, 192, 172)},
};
static const char *const SKIN_NAME[] = {"GREEN", "AMBER", "CYAN", "WHITE", "RED"};
#define NSKINS (sizeof SKINS / sizeof SKINS[0])
#define SKIN(i) SKINS[set.skin < NSKINS ? set.skin : 0][i]
#define G_BG SKIN(0)
#define G_FAINT SKIN(1)
#define G_DIM SKIN(2)
#define G_MID SKIN(3)
#define G_HI SKIN(4)
#define G_GLOW SKIN(5)

#define HEAD_H 22
#define BODY_Y 24
#define BODY_H 192
#define FOOT_Y 218
#define FOOT_H 22
#define LINE_H 12                      /* FONT_XS (Terminus 6 x 12) */
#define NLINES (BODY_H / LINE_H)
#define CW 6                           /* FONT_XS's advance */

enum { TAB_LOG, TAB_FILTER, TAB_STATS, TAB_THRU, TAB_GLO, NTABS };
static const char *const TAB_NAME[NTABS] = {"LOG", "FILTER", "STATS", "THRU", "GLO"};
/* what the LOG tab shows (ALGO; views.c): its name stands in the tab's place */
enum { VIEW_LOG, VIEW_KEYS, VIEW_CC, VIEW_CHANS, VIEW_CLOCK, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"LOG", "KEYS", "CC", "CHANS", "CLOCK"};
static const char *tab_name(uint32_t tab) { return tab == TAB_LOG ? VIEW_NAME[set.view] : TAB_NAME[tab]; }
static void views_draw(void);
static const char *const OUT_NAME[NOUTS] = {"USB", "BLE-P", "BLE-C"};
static const char *const FMT_NAME[3] = {"HEX", "HUMAN", "BOTH"};
static const char *const TIME_NAME[3] = {"+MS", "ABS", "NO T"};

/* the rows a little darker every other line, as a CRT's */
static void scanlines(void)
{
    uint32_t x, y;
    if (!set.scan)
        return;
    for (y = 1; y < cv_h; y += 2)
        for (x = 0; x < cv_w; x++) {
            uint16_t *p = &cv_px[y * cv_w + x];
            uint32_t c = swap16(*p);
            *p = swap16(((c >> 1) & 0x7BEFu) + ((c >> 2) & 0x39E7u));
        }
}

static void text_inv(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)   /* lit block */
{
    cv_rect(x - 2, y, text_w(f, s) + 4, f->h, c);
    cv_text(x, y, f, s, G_BG);
}

/* ---------------------------------------------------------------- header --- */
static void draw_header(void)
{
    int32_t x = 4, i, gap = 0;
    cv_begin(240, HEAD_H, G_BG);
    cv_rect(0, HEAD_H - 3, 240, 1, G_MID);
    for (i = 0; i < NTABS; i++)        /* the gaps: as wide as the names leave room for (a long view's name) */
        gap += text_w(&FONT_S, tab_name((uint32_t)i));
    gap = (240 - 16 - gap) / (NTABS - 1);
    if (gap > 12)
        gap = 12;
    for (i = 0; i < NTABS; i++) {
        const char *name = tab_name((uint32_t)i);
        int32_t w = text_w(&FONT_S, name);
        if (i == (int32_t)ui.tab) {    /* the page we are on: lit, in brackets that stand on the line */
            cv_rect(x - 4, 3, 1, HEAD_H - 5, G_MID);
            cv_rect(x + w + 3, 3, 1, HEAD_H - 5, G_MID);
            cv_rect(x - 4, 3, 3, 1, G_MID);
            cv_rect(x + w + 1, 3, 3, 1, G_MID);
            cv_rect(x - 3, HEAD_H - 3, w + 6, 1, G_BG);
            cv_text(x, 2, &FONT_S, name, G_HI);
        } else {
            cv_text(x, 2, &FONT_S, name, G_DIM);
        }
        x += w + gap;
    }
    scanlines();
    cv_blit(0, 0);
}

/* ---------------------------------------------------------------- footer --- */
static void draw_footer(void)
{
    uint32_t s, now = plat_ms();
    int32_t x = 4;
    char t[24], *d;
    cv_begin(240, FOOT_H, G_BG);
    cv_rect(0, 1, 240, 1, G_MID);
    for (s = 0; s < NTAPS; s++) {      /* the sources: lit as messages come, dim when hidden */
        int shown = set.srcs >> s & 1u, busy = ui.act[s] && now - ui.act[s] < 90u;
        const char *nm = s == TAP_USB && plat_usb_host_state() != USBH_OFF ? "HOST" : SRC_NAME[s];   /* (USB host) */
        if (shown && busy)
            text_inv(x, 6, &FONT_XS, nm, G_HI);
        else
            cv_text(x, 6, &FONT_XS, nm, shown ? G_MID : G_FAINT);
        if (!shown)
            cv_rect(x - 1, 12, text_w(&FONT_XS, nm) + 2, 1, G_DIM);
        x += text_w(&FONT_XS, nm) + CW;
    }
    if (ui.paused || ui.scroll) {      /* the view stopped: paused, or scrolled back so many lines */
        if (ui.paused)
            s_cat(t, "PAUSED");
        else {
            t[0] = '-';
            s_dec(t + 1, (int32_t)ui.scroll, 0);
        }
        text_inv(240 - 4 - text_w(&FONT_XS, t), 6, &FONT_XS, t, G_HI);
    } else {
        if (set.view == VIEW_LOG) {
            d = s_cat(t, FMT_NAME[set.fmt]);
            *d++ = ' ';
            d = s_cat(d, TIME_NAME[set.tmode]);
        } else {                       /* another view: which, of how many (ALGO turns them) */
            d = s_cat(t, "ALGO ");
            d = s_dec(d, (int32_t)set.view + 1, 0);
            *d++ = '/';
            d = s_dec(d, NVIEWS, 0);
        }
        if (set.thru)
            s_cat(d, " THRU");
        cv_text(240 - 4 - text_w(&FONT_XS, t), 6, &FONT_XS, t, G_DIM);
    }
    scanlines();
    cv_blit(0, FOOT_Y);
}

/* ------------------------------------------------------------------- LOG --- */
static int ev_shown(const ev_t *e)
{
    if (!(set.srcs >> e->src & 1u))
        return 0;
    if (e->type != TY_ERR && !(set.types >> e->type & 1u))
        return 0;
    if ((e->kind == EV_MSG || e->kind == EV_RS) && e->b[0] < 0xF0u && !(set.chans >> (e->b[0] & 15u) & 1u))
        return 0;
    return 1;
}

/* the shown events, newest last: up to n of them, the newest skip back from head; out[0] the one before the first
 * (for its time difference; -1: none). Returns how many */
static int log_collect(uint32_t head, uint32_t skip, int32_t *out, int n)
{
    uint32_t i = head, lo = ev_oldest();
    int got = 0;
    int32_t tmp[NLINES + 1];
    while (i > lo && got < n + 1) {
        const ev_t *e = ev_at(--i);
        if (!ev_shown(e))
            continue;
        if (skip) {
            skip--;
            continue;
        }
        tmp[got++] = (int32_t)i;
    }
    {
        int k, shown = got > n ? n : got;
        out[0] = got > n ? tmp[n] : -1;
        for (k = 0; k < shown; k++)
            out[1 + k] = tmp[shown - 1 - k];
        return shown;
    }
}

static uint32_t view_cuts;              /* log lines cut to fit (the tests: none) */
static int32_t view_maxx;              /* the furthest a log line reached (the tests: within 240) */

static void draw_line(int32_t y, const ev_t *e, const ev_t *prev)
{
    char t[48], *d;
    int32_t x = 2;
    int err = e->type == TY_ERR, real = e->type == TY_CLOCK || e->type == TY_ACTIVE;
    uint16_t hi = real ? G_DIM : e->type == TY_SYSEX ? G_GLOW : G_HI, mid = real ? G_DIM : G_MID;
    if (err)
        cv_rect(0, y, 240, LINE_H, G_MID);
    if (set.tmode == 0) {              /* milliseconds since the line above */
        if (prev) {
            uint32_t dt = e->ms - prev->ms;
            t[0] = '+';
            if (dt > 9999u)
                s_cat(t, ">10s");
            else
                s_dec(t + 1, (int32_t)dt, 4);
        } else {
            s_cat(t, "     ");
        }
        cv_text(x, y, &FONT_XS, t, err ? G_BG : G_DIM);
        x += 6 * CW;
    } else if (set.tmode == 1) {       /* seconds since power-on, the last 1000 */
        d = s_dec(t, (int32_t)(e->ms / 1000u % 1000u), 3);
        *d++ = '.';
        d = s_dec(d, (int32_t)(e->ms % 1000u), 3);
        for (d = t + 4; d < t + 7; d++)
            if (*d == ' ')
                *d = '0';
        cv_text(x, y, &FONT_XS, t, err ? G_BG : G_DIM);
        x += 8 * CW;
    }
    cv_text(x, y, &FONT_XS, SRC_NAME[e->src], err ? G_BG : G_MID);
    x += 6 * CW;
    if (set.fmt != 1) {                /* the bytes; a running status's own byte (not on the wire) dim */
        int max = set.fmt == 2 ? 8 : (240 - x) / CW;
        int32_t end;
        ev_hex(e, t, max);
        if (e->kind == EV_RS) {
            char c = t[2];
            t[2] = 0;
            cv_text(x, y, &FONT_XS, t, G_FAINT);
            t[2] = c;
            end = cv_text(x + 2 * CW, y, &FONT_XS, t + 2, mid);
        } else {
            end = cv_text(x, y, &FONT_XS, t, err ? G_BG : mid);
        }
        if (end > view_maxx)
            view_maxx = end;
        if (set.fmt == 0)
            return;
        x += 9 * CW;
    }
    {
        int room = (240 - (int)x - 3 * CW) / CW, ch = ev_human(e, t, set.midc, room);
        int32_t end;
        if (ch) {
            char c[4];
            s_dec(c, ch, 2);
            cv_text(x, y, &FONT_XS, c, err ? G_BG : G_DIM);
        }
        x += 3 * CW;
        if (s_len(t) > room) {         /* (should not happen: the short forms fit; ~ marks it if it does) */
            t[room - 1] = '~';
            t[room] = 0;
            view_cuts++;
        }
        end = cv_text(x, y, &FONT_XS, t, err ? G_BG : hi);
        if (end > view_maxx)
            view_maxx = end;
    }
}

static void draw_log(void)
{
    int32_t idx[NLINES + 1];
    uint32_t head = ui.paused || ui.scroll ? ui.head : ev_w;
    int n = log_collect(head, ui.scroll, idx, NLINES), k;
    int32_t y = (NLINES - n) * LINE_H;
    if (!n) {
        cv_text(8, BODY_H / 2 - 20, &FONT_S, "> WAITING FOR MIDI_", (plat_ms() / 500u) & 1u ? G_HI : G_MID);
        cv_text(8, BODY_H / 2 + 2, &FONT_XS, "USB  TRS  BLUETOOTH", G_DIM);
        return;
    }
    for (k = 0; k < n; k++, y += LINE_H)
        draw_line(y, ev_at((uint32_t)idx[1 + k]), k ? ev_at((uint32_t)idx[k]) : idx[0] >= 0 ? ev_at((uint32_t)idx[0]) : 0);
}

/* ---------------------------------------------------------------- FILTER --- */
/* a filter's channels and messages, the keys that toggle them; route: a route's (else the display's) */
static void draw_filter_of(uint32_t chans, uint32_t types, int route)
{
    uint32_t i;
    char t[8];
    cv_text(4, 2, &FONT_XS, "CHANNELS  (WHITE KEYS)", G_MID);
    for (i = 0; i < 16u; i++) {        /* a cell a channel, lit when it passes */
        int32_t x = 4 + (int32_t)(i % 8u) * 29, y = 18 + (int32_t)(i / 8u) * 18, on = chans >> i & 1u;
        s_dec(t, (int32_t)i + 1, 0);
        cv_rect(x, y, 26, 15, on ? G_MID : G_FAINT);
        cv_text(x + (26 - text_w(&FONT_XS, t)) / 2, y + 2, &FONT_XS, t, on ? G_BG : G_DIM);
    }
    cv_text(4, 60, &FONT_XS, "MESSAGES  (BLACK KEYS)", G_MID);
    for (i = 0; i < NTY; i++) {
        int32_t x = 4 + (int32_t)(i / 6u) * 118, y = 76 + (int32_t)(i % 6u) * 14, on = types >> i & 1u;
        cv_text(x, y, &FONT_XS, on ? "[X]" : "[ ]", on ? G_HI : G_DIM);
        cv_text(x + 4 * CW, y, &FONT_XS, TY_NAME[i], on ? G_HI : G_DIM);
    }
    if (route) {
        cv_text(4, 164, &FONT_XS, "K1-3 OUT OCT- NONE OCT+ ALL EDIT=BACK", G_DIM);
    } else {
        cv_text(4, 164, &FONT_XS, "FX USB  SEL TRS  ENV BLE-P  LFO BLE-C", G_DIM);
        cv_text(4, 178, &FONT_XS, "OCT- NONE  OCT+ ALL  EDIT DEFAULTS", G_DIM);
    }
}
static void draw_filter(void) { draw_filter_of(set.chans, set.types, 0); }

static char *addr_tail(char *d, const uint8_t *a)    /* "EE:FF": a Bluetooth address's last four digits */
{
    d = s_hex2(d, a[4]);
    *d++ = ':';
    return s_hex2(d, a[5]);
}

/* The battery (GLO 1/3), an estimate: the converter's reading (plat_battery_raw, 10 bits) taken against the 3.2 V
 * I/O supply through a 1:2 divider (an assumption, not measured; it puts the stock firmware's bar thresholds 531 561
 * 591 at 3.32 3.51 3.70 V), the volts then a percentage on a lithium cell's discharge curve. mV, and percent (-1: no
 * reading yet) */
static uint32_t batt_mv(void) { return (uint32_t)plat_battery_raw() * 6400u / 1023u; }
static int batt_percent(void)
{
    static const uint16_t MV[] = {3300, 3500, 3600, 3650, 3700, 3750, 3800, 3900, 4000, 4100, 4200};
    static const uint8_t PC[] = {0, 3, 8, 15, 25, 40, 50, 65, 80, 90, 100};
    uint32_t mv = batt_mv(), i;
    if (!plat_battery_raw())
        return -1;
    if (mv <= MV[0])
        return 0;
    for (i = 1; i < sizeof MV / sizeof MV[0]; i++)
        if (mv < MV[i])
            return PC[i - 1] + (int)((mv - MV[i - 1]) * (uint32_t)(PC[i] - PC[i - 1]) / (uint32_t)(MV[i] - MV[i - 1]));
    return 100;
}

/* Bluetooth's state in words (STATS, GLO 2/3): the radio; BLE-P's computer; BLE-C's device */
static const char *bt_radio_word(void)
{
    return !FM1B_BLE ? "NOT BUILT" : plat_ble_state() == BLE_OFF ? "OFF" : plat_ble_state() == BLE_READY ? "ON" :
           plat_ble_state() == BLE_FAILED ? "FAILED" : "STARTING";
}
static const char *bt_p_word(void)
{
    return !set.ble_p ? "OFF" : plat_ble_connected() ? "COMPUTER CONNECTED" : plat_ble_linked() ? "COMPUTER LINKED" :
           plat_ble_state() == BLE_READY ? "WAITING" : "--";
}
static const char *bt_c_word(void)       /* (connected: the device's name, from the line below) */
{
    static const char *const DEV[] = {"IDLE", "SCANNING", "CONNECTING", "CONNECTING", "CONNECTED"};
    int st = plat_ble_device();
    return set.ble_c == BLEC_OFF ? "OFF" : plat_ble_state() != BLE_READY ? "--" :
           st >= 0 && st <= BLE_DEV_READY ? DEV[st] : "?";
}

/* Bluetooth's state, three lines from y (STATS: the short pair); returns the y after them */
static int32_t draw_bt_state(int32_t y)
{
    cv_text(4, y, &FONT_XS, "BLUETOOTH", G_MID);
    cv_text(4 + 11 * CW, y, &FONT_XS, bt_radio_word(), G_HI);
    y += 14;
    cv_text(4, y, &FONT_XS, " BLE-P", G_DIM);
    cv_text(4 + 11 * CW, y, &FONT_XS, bt_p_word(), G_HI);
    y += 14;
    cv_text(4, y, &FONT_XS, " BLE-C", G_DIM);
    cv_text(4 + 11 * CW, y, &FONT_XS, plat_ble_device() == BLE_DEV_READY ? ui.cdev_name : bt_c_word(), G_HI);
    return y + 14;
}

/* ------------------------------------------------------------------ THRU --- */
static void draw_thru(void)
{
    static const char *const KNOB[NOUTS] = {"K1", "K2", "K3"};
    uint32_t s, o, now = plat_ms();
    int32_t y = 2, x0 = 4 + 7 * CW, cw = 7 * CW;
    char t[24], *d;
    if (ui.rt_edit) {                  /* a route's filter: what of SOURCE goes to OUTPUT */
        uint32_t so = ui.thru_row, oo = ui.rt_out;
        cv_band(14, BODY_H - 14);      /* (the filter's layout, a line lower: the route on top) */
        draw_filter_of(set.rt_chans[so][oo], set.rt_types[so][oo], 1);
        cv_band(0, BODY_H);
        d = s_cat(t, SRC_NAME[so]);
        d = s_cat(d, " -> ");
        s_cat(d, OUT_NAME[oo]);
        text_inv(6, 0, &FONT_XS, t, G_HI);
        if (SRC_SELF[so] >> oo & 1u)
            cv_text(8 + text_w(&FONT_XS, t) + 6, 0, &FONT_XS, "(NOT A ROUTE)", G_DIM);
        else if (!(set.routes[so] >> oo & 1u))
            cv_text(8 + text_w(&FONT_XS, t) + 6, 0, &FONT_XS, "(ROUTE OFF)", G_DIM);
        return;
    }
    cv_text(4, y, &FONT_XS, "THRU", G_MID);
    if (set.thru)
        text_inv(4 + 6 * CW, y, &FONT_XS, "ON", G_HI);
    else
        cv_text(4 + 6 * CW, y, &FONT_XS, "OFF", G_DIM);
    cv_text(4 + 12 * CW, y, &FONT_XS, "(K4)", G_DIM);
    y += 20;
    cv_text(4, y, &FONT_XS, "FROM", G_DIM);
    for (o = 0; o < NOUTS; o++) {      /* the outputs across, each with its knob */
        cv_text(x0 + (int32_t)o * cw, y - 8, &FONT_XS, KNOB[o], G_DIM);
        cv_text(x0 + (int32_t)o * cw, y + 4, &FONT_XS, OUT_NAME[o], G_MID);
    }
    cv_text(x0 + 3 * cw, y + 4, &FONT_XS, "SENT", G_DIM);
    y += 22;
    for (s = 0; s < NTAPS; s++, y += 18) {   /* the sources down: SELECT picks one */
        int row = s == ui.thru_row;
        if (row)
            cv_rect(0, y - 3, 240, 17, G_FAINT);
        cv_text(4, y, &FONT_XS, SRC_NAME[s], row ? G_HI : G_MID);
        for (o = 0; o < NOUTS; o++) {
            int32_t x = x0 + (int32_t)o * cw + 6;
            int on = set.routes[s] >> o & 1u, busy = set.thru && ui.route_ms[s][o] && now - ui.route_ms[s][o] < 100u;
            if (SRC_SELF[s] >> o & 1u)
                cv_text(x, y, &FONT_XS, " -", G_FAINT);
            else if (busy && on)
                text_inv(x, y, &FONT_XS, "[>]", G_GLOW);
            else
                cv_text(x, y, &FONT_XS, !on ? "[ ]" : rt_filtered(s, o) ? "[F]" : "[X]",
                        on ? (set.thru ? G_HI : G_MID) : G_DIM);
        }
        s_dec(t, (int32_t)ui.routed[s], 6);
        cv_text(x0 + 3 * cw - CW, y, &FONT_XS, t, G_DIM);
    }
    y += 10;
    cv_text(4, y, &FONT_XS, "SELECT SOURCE  K1-K3 ITS OUTPUTS", G_DIM);
    cv_text(4, y + 14, &FONT_XS, "EDIT A ROUTE'S FILTER  [F] FILTERED", G_DIM);
    cv_text(4, y + 28, &FONT_XS, "SYSEX GOES TO USB ONLY (FOR NOW)", G_FAINT);
}

/* ------------------------------------------------------------------- GLO --- */
static void draw_glo_ble(void)           /* GLO 2/3: Bluetooth (ble.c), both roles */
{
    static const char *const LABEL[4] = {"K1 BLE-PERIPHERY", "K2 BLE-CENTRAL", "K3 CONNECT TO", "K4 RECONNECT"};
    char dev[28], *d;
    const char *val[4];
    int32_t y = 2, i;
    if (set.ble_have) {                  /* the chosen one: its name (14 at most here) and the end of its address */
        for (i = 0, d = dev; i < 14 && set.ble_name[i]; i++)
            *d++ = set.ble_name[i];
        *d++ = ' ';
        addr_tail(d, set.ble_addr);
    } else {
        s_cat(dev, set.ble_c == BLEC_CHOOSE ? "TURN: THE LIST" : "--");
    }
    val[0] = set.ble_p ? "ON" : "OFF";
    val[1] = BLEC_NAME[set.ble_c % 3u];
    val[2] = dev;
    val[3] = set.ble_auto ? "ON" : "OFF";
    for (i = 0; i < 4; i++, y += 15) {
        cv_text(4, y, &FONT_XS, LABEL[i], G_MID);
        cv_text(4 + 18 * CW, y, &FONT_XS, val[i], i == 2 && !set.ble_have ? G_DIM : G_HI);
    }
    y += 4;
    cv_rect(4, y, 232, 1, G_FAINT);
    y += 8;
    cv_text(4, y, &FONT_XS, "RADIO", G_DIM);
    cv_text(4 + 15 * CW, y, &FONT_XS, bt_radio_word(), G_HI);
    y += 14;
    cv_text(4, y, &FONT_XS, "BLE-PERIPHERY", G_DIM);
    cv_text(4 + 15 * CW, y, &FONT_XS, bt_p_word(), G_HI);
    y += 14;
    cv_text(4, y, &FONT_XS, "BLE-CENTRAL", G_DIM);
    cv_text(4 + 15 * CW, y, &FONT_XS, bt_c_word(), G_HI);
    y += 14;
    if (plat_ble_device() == BLE_DEV_CONNECTING || plat_ble_device() == BLE_DEV_SEARCHING ||
        plat_ble_device() == BLE_DEV_READY) {   /* the device it is on: name and address's end */
        d = s_cat(dev, ui.cdev_name);
        *d++ = ' ';
        addr_tail(d, ui.cdev_addr);
        cv_text(4 + 15 * CW, y, &FONT_XS, dev, G_GLOW);
    }
    y += 22;
    cv_text(4, y, &FONT_XS, "FIRST: THE FIRST ONE SEEN", G_FAINT);
    cv_text(4, y + 13, &FONT_XS, "CHOOSE: THE ONE CHOSEN (K3)", G_FAINT);
}

/* the device list over GLO 2/3 (ble.c): the scan's devices, the pick lit, the chosen one marked */
static void draw_picker(void)
{
    char t[40], name[21], *d;
    int n = 0, i, rssi;
    uint8_t a[6];
    cv_rect(6, 4, 228, 184, G_BG);
    cv_rect(6, 4, 228, 1, G_HI);
    cv_rect(6, 187, 228, 1, G_HI);
    cv_rect(6, 4, 1, 184, G_HI);
    cv_rect(233, 4, 1, 184, G_HI);
    while (n < 8 && plat_ble_found(n, 0, 0))
        n++;
    d = s_cat(t, "BLE-CENTRAL: CHOOSE  (");
    d = s_dec(d, n, 0);
    s_cat(d, ")");
    cv_text(12, 9, &FONT_XS, t, G_HI);
    if (!n)
        cv_text(14, 40, &FONT_XS, (plat_ms() / 500u) & 1u ? "SCANNING_" : "SCANNING", G_DIM);
    for (i = ui.pick_top; i < n && i < ui.pick_top + PICK_ROWS; i++) {
        int32_t y = 28 + (i - ui.pick_top) * 15;
        plat_ble_found(i, name, &rssi);
        plat_ble_found_addr(i, a);
        d = s_cat(t, set.ble_have && !memcmp(a, set.ble_addr, 6) ? "*" : " ");
        d = s_cat(d, name);
        d = s_pad(d, t, 22);
        d = addr_tail(d, a);
        d = s_dec(d, rssi, 5);
        if (i == ui.pick)
            text_inv(12, y, &FONT_XS, t, G_HI);
        else
            cv_text(12, y, &FONT_XS, t, G_MID);
    }
    if (ui.pick_top)                     /* more above / below */
        cv_text(222, 28, &FONT_XS, "^", G_DIM);
    if (ui.pick_top + PICK_ROWS < n)
        cv_text(222, 28 + (PICK_ROWS - 1) * 15, &FONT_XS, "v", G_DIM);
    cv_text(12, 146, &FONT_XS, "* THE CHOSEN ONE", G_FAINT);
    cv_text(12, 158, &FONT_XS, "OCT+ CONNECT  OCT- BACK", G_MID);
    cv_text(12, 171, &FONT_XS, "PLAY LOOK AGAIN  K3/SELECT MOVE", G_MID);
}

static void draw_glo2(void)              /* GLO 3/3: the look, what the log keeps */
{
    int32_t y = 2;
    uint32_t i;
    cv_text(4, y, &FONT_XS, "K1 SKIN", G_MID);
    cv_text(4 + 14 * CW, y, &FONT_XS, SKIN_NAME[set.skin < NSKINS ? set.skin : 0], G_HI);
    y += 15;
    cv_text(4, y, &FONT_XS, "K2 PLATFORM SX", G_MID);
    cv_text(4 + 15 * CW, y, &FONT_XS, set.show_sys ? "LOGGED" : "HIDDEN", G_HI);
    y += 15;
    cv_text(4, y, &FONT_XS, "K3 SCANLINES", G_MID);
    cv_text(4 + 15 * CW, y, &FONT_XS, set.scan ? "ON" : "OFF", G_HI);
    y += 22;
    for (i = 0; i < NSKINS; i++) {       /* every skin's shades, the one in use marked */
        uint32_t c;
        int32_t x = 4 + (int32_t)i * 46;
        for (c = 0; c < 6u; c++)
            cv_rect(x + (int32_t)c * 7, y, 6, 14, SKINS[i][c]);
        if (i == set.skin)
            cv_rect(x, y + 17, 41, 2, G_HI);
    }
    y += 32;
    cv_text(4, y, &FONT_XS, "PLATFORM SX: THE FM-1'S OWN SYSEX OVER", G_DIM);
    cv_text(4, y + 13, &FONT_XS, "USB (SCREEN READER, REMOTE, UPDATES).", G_DIM);
    cv_text(4, y + 26, &FONT_XS, "HIDDEN: NOT IN THE LOG OR ITS COUNTS.", G_DIM);
    cv_text(4, y + 46, &FONT_XS, "GLO: BACK", G_FAINT);
}

static void draw_glo(void)               /* GLO 1/3 (note 60, the USB port), 2/3 Bluetooth, 3/3 the look */
{
    static const char *const LABEL[2] = {"K1 NOTE 60", "K2 USB PORT"};
    static const char *const USBH[3] = {"DEVICE MODE", "HOST MODE: WAITING", "HOST MODE: PLAYING"};
    static const char *const PAGE[3] = {"1/3", "BLUETOOTH 2/3", "3/3"};
    const char *val[2];
    int32_t y = 2, i;
    cv_text(240 - 4 - text_w(&FONT_XS, PAGE[ui.glo_page % 3u]), ui.glo_page == 1u ? 180 : 2, &FONT_XS,
            PAGE[ui.glo_page % 3u], G_DIM);
    if (ui.glo_page == 1u) {
        draw_glo_ble();
        return;
    }
    if (ui.glo_page) {
        draw_glo2();
        return;
    }
    val[0] = set.midc == 3u ? "C3 (YAMAHA)" : "C4 (ROLAND)";
    val[1] = USBH[plat_usb_host_state() % 3];
    for (i = 0; i < 2; i++, y += 15) {
        cv_text(4, y, &FONT_XS, LABEL[i], G_MID);
        cv_text(4 + 14 * CW, y, &FONT_XS, val[i], G_HI);
    }
    y += 8;
    {   /* the battery: the percentage, a bar, and what it comes from (an estimate: batt_percent) */
        char t[40], *d;
        int pc = batt_percent();
        uint32_t mv = batt_mv();
        uint16_t c = pc < 0 ? G_DIM : pc <= 15 ? G_GLOW : G_HI;
        cv_text(4, y + 6, &FONT_XS, "BATTERY", G_MID);
        if (pc < 0) {
            s_cat(t, "--");
        } else {
            d = s_dec(t, pc, 0);
            s_cat(d, "%");
        }
        cv_text(64, y, &FONT_M, t, c);
        cv_rect(128, y + 5, 104, 14, G_DIM);   /* the bar: an outline, filled to the percentage */
        cv_rect(129, y + 6, 102, 12, G_BG);
        if (pc > 0)
            cv_rect(130, y + 7, pc, 10, c);
        y += 28;
        d = s_cat(t, "~");
        d = s_dec(d, (int32_t)(mv / 1000u), 0);
        *d++ = '.';
        d = s_dec(d, (int32_t)(mv % 1000u / 10u), 2);
        if (d[-2] == ' ')
            d[-2] = '0';
        d = s_cat(d, " V  RAW ");
        d = s_dec(d, plat_battery_raw(), 0);
        s_cat(d, plat_battery() == 4 ? "  ON USB POWER" : "  ESTIMATE");
        cv_text(4, y, &FONT_XS, t, G_DIM);
        y += 18;
    }
    cv_text(4, y, &FONT_XS, "USB PORT", G_MID);
    cv_text(4, y + 14, &FONT_XS, " DEVICE MODE: A COMPUTER'S, AS USUAL", G_DIM);
    cv_text(4, y + 27, &FONT_XS, " HOST MODE: A USB MIDI CONTROLLER", G_DIM);
    cv_text(4, y + 40, &FONT_XS, "   OR SYNTH ON IT, POWERED BY A HUB", G_DIM);
    cv_text(4, y + 53, &FONT_XS, "   OR AN OTG ADAPTER", G_DIM);
    cv_text(4, y + 66, &FONT_XS, " EVERY POWER-ON: DEVICE MODE", G_DIM);
    cv_text(4, y + 84, &FONT_XS, "GLO: BLUETOOTH, THEN THE LOOK", G_FAINT);
}

/* ----------------------------------------------------------------- STATS --- */
static void draw_stats(void)
{
    uint32_t s;
    char t[48], *d;
    int32_t y = 2;
    cv_text(4, y, &FONT_XS, "SRC    EVENTS   /S   BYTES  LOST", G_MID);
    for (s = 0; s < NTAPS; s++) {
        y += 14;
        d = s_cat(t, SRC_NAME[s]);
        d = s_pad(d, t, 5);
        d = s_dec(d, (int32_t)src_events[s], 8);
        d = s_dec(d, (int32_t)ui.rate[s], 5);
        d = s_dec(d, (int32_t)plat_tap_bytes((int)s), 8);
        s_dec(d, (int32_t)src_lost_seen[s], 6);
        cv_text(4, y, &FONT_XS, t, set.srcs >> s & 1u ? G_HI : G_DIM);
    }
    y += 16;
    d = s_cat(t, "SENT  USB");          /* what went out: thru, clock (plat_midi_count) */
    d = s_dec(d, (int32_t)plat_midi_count(PORT_USB, 1), 8);
    d = s_cat(d, "  BLE");
    s_dec(d, (int32_t)plat_midi_count(PORT_BLE, 1), 8);
    cv_text(4, y, &FONT_XS, t, G_HI);
    y += 20;
    y = draw_bt_state(y);
    y += 8;
    cv_text(4, y, &FONT_XS, "CLOCK", G_MID);
    if (ui.bpm10 && plat_ms() - ui.clk_ms < 1000u) {
        d = s_dec(t, (int32_t)(ui.bpm10 / 10u), 0);
        *d++ = '.';
        d = s_dec(d, (int32_t)(ui.bpm10 % 10u), 0);
        d = s_cat(d, " BPM  ");
        d = s_cat(d, SRC_NAME[ui.clk_src]);
        s_cat(d, ui.running ? "  RUNNING" : "  STOPPED");
    } else {
        s_cat(t, "--");
    }
    cv_text(4 + 11 * CW, y, &FONT_XS, t, G_HI);
    y += 22;
    d = s_cat(t, "LOG ");
    d = s_dec(d, (int32_t)(ev_w - ev_oldest()), 0);
    d = s_cat(d, "/");
    s_dec(d, EV_RING, 0);
    cv_text(4, y, &FONT_XS, t, G_DIM);
}

/* ------------------------------------------------------------------ body --- */
static void draw_body(void)
{
    cv_begin(240, BODY_H, G_BG);
    if (ui.tab == TAB_LOG && set.view != VIEW_LOG)
        views_draw();
    else if (ui.tab == TAB_LOG)
        draw_log();
    else if (ui.tab == TAB_FILTER)
        draw_filter();
    else if (ui.tab == TAB_STATS)
        draw_stats();
    else if (ui.tab == TAB_THRU)
        draw_thru();
    else
        draw_glo();
    if (ui.tab == TAB_GLO && ui.glo_page == 1u && ui.picker)
        draw_picker();                 /* the device list, over its page */
    if (ui.ask) {                      /* a question: OCT- NO on the left, OCT+ YES on the right */
        char q[32] = "", name[28] = "";
        if (ui.ask == ASK_CLEAR) {
            s_cat(q, "CLEAR THE LOG?");
        } else if (ui.ask == ASK_USBH) {   /* (and what the port stops being) */
            s_cat(q, plat_usb_host_state() == USBH_OFF ? "USB: HOST MODE?" : "USB: DEVICE MODE?");
            s_cat(name, plat_usb_host_state() == USBH_OFF ? "DISCONNECTS THE COMPUTER" : "BACK TO THE COMPUTER");
        }
        cv_rect(20, 60, 200, 64, G_BG);
        cv_rect(20, 60, 200, 1, G_HI);
        cv_rect(20, 123, 200, 1, G_HI);
        cv_rect(20, 60, 1, 64, G_HI);
        cv_rect(219, 60, 1, 64, G_HI);
        cv_text(120 - text_w(&FONT_S, q) / 2, name[0] ? 66 : 72, &FONT_S, q, G_HI);
        if (name[0])
            cv_text(120 - text_w(&FONT_XS, name) / 2, 84, &FONT_XS, name, G_GLOW);
        cv_text(28, 102, &FONT_XS, "< OCT- NO", G_MID);
        cv_text(212 - text_w(&FONT_XS, "YES OCT+ >"), 102, &FONT_XS, "YES OCT+ >", G_MID);
    }
    if (ui.msg_until && plat_ms() < ui.msg_until) {   /* a message, a moment: the bottom line */
        char t[52], *d = s_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            *d++ = ' ';
            s_cat(d, ui.msg[1]);
        }
        cv_rect(0, BODY_H - LINE_H - 2, 240, LINE_H + 2, G_HI);
        cv_text(4, BODY_H - LINE_H - 1, &FONT_XS, t, G_BG);
    }
    scanlines();
    cv_blit(0, BODY_Y);
}
