/* SPDX-License-Identifier: MPL-2.0 */
/* The screens, in a green phosphor terminal's look: the tabs (header), the page (body: LOG, FILTER, STATS) and the
 * sources (footer). Part of app.c, after mon.c. */

/* the phosphor's shades */
#define G_BG RGB(2, 12, 6)
#define G_FAINT RGB(14, 52, 28)
#define G_DIM RGB(30, 104, 56)
#define G_MID RGB(56, 184, 96)
#define G_HI RGB(112, 255, 150)
#define G_GLOW RGB(196, 255, 214)

#define HEAD_H 22
#define BODY_Y 24
#define BODY_H 192
#define FOOT_Y 218
#define FOOT_H 22
#define LINE_H 12                      /* FONT_XS (Terminus 6 x 12) */
#define NLINES (BODY_H / LINE_H)
#define CW 6                           /* FONT_XS's advance */

enum { TAB_LOG, TAB_FILTER, TAB_STATS, TAB_GLO, NTABS };
static const char *const TAB_NAME[NTABS] = {"LOG", "FILTER", "STATS", "GLO"};
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
    int32_t x = 4, i;
    char t[16];
    cv_begin(240, HEAD_H, G_BG);
    cv_rect(0, HEAD_H - 3, 240, 1, G_MID);
    for (i = 0; i < NTABS; i++) {
        int32_t w = text_w(&FONT_S, TAB_NAME[i]);
        if (i == (int32_t)ui.tab) {    /* the page we are on: lit, in brackets that stand on the line */
            cv_rect(x - 4, 3, 1, HEAD_H - 5, G_MID);
            cv_rect(x + w + 3, 3, 1, HEAD_H - 5, G_MID);
            cv_rect(x - 4, 3, 3, 1, G_MID);
            cv_rect(x + w + 1, 3, 3, 1, G_MID);
            cv_rect(x - 3, HEAD_H - 3, w + 6, 1, G_BG);
            cv_text(x, 2, &FONT_S, TAB_NAME[i], G_HI);
        } else {
            cv_text(x, 2, &FONT_S, TAB_NAME[i], G_DIM);
        }
        x += w + 12;
    }
    if (ui.paused)
        text_inv(240 - 4 - text_w(&FONT_XS, "PAUSED"), 4, &FONT_XS, "PAUSED", (plat_ms() / 500u) & 1u ? G_HI : G_MID);
    else if (ui.scroll) {
        t[0] = '-';
        s_dec(t + 1, (int32_t)ui.scroll, 0);
        cv_text(240 - 4 - text_w(&FONT_XS, t), 4, &FONT_XS, t, G_HI);
    } else {
        cv_text(240 - 4 - text_w(&FONT_XS, "LIVE"), 4, &FONT_XS, "LIVE", G_DIM);
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
        if (shown && busy)
            text_inv(x, 6, &FONT_XS, SRC_NAME[s], G_HI);
        else
            cv_text(x, 6, &FONT_XS, SRC_NAME[s], shown ? G_MID : G_FAINT);
        if (!shown)
            cv_rect(x - 1, 12, 3 * CW + 2, 1, G_DIM);
        x += 4 * CW;
    }
    d = s_cat(t, FMT_NAME[set.fmt]);
    *d++ = ' ';
    d = s_cat(d, TIME_NAME[set.tmode]);
    d = s_cat(d, set.midc == 3 ? " C3" : " C4");
    if (set.thru)
        s_cat(d, " THRU");
    cv_text(240 - 4 - text_w(&FONT_XS, t), 6, &FONT_XS, t, G_DIM);
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
            if (dt > 99999u)
                s_cat(t + 1, ">99s");
            else
                s_dec(t + 1, (int32_t)dt, 5);
        } else {
            s_cat(t, "      ");
        }
        cv_text(x, y, &FONT_XS, t, err ? G_BG : G_DIM);
        x += 7 * CW;
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
    x += 4 * CW;
    if (set.fmt != 1) {                /* the bytes; a running status's own byte (not on the wire) dim */
        int max = set.fmt == 2 ? 8 : (240 - x) / CW;
        ev_hex(e, t, max);
        if (e->kind == EV_RS) {
            char c = t[2];
            t[2] = 0;
            cv_text(x, y, &FONT_XS, t, G_FAINT);
            t[2] = c;
            cv_text(x + 2 * CW, y, &FONT_XS, t + 2, mid);
        } else {
            cv_text(x, y, &FONT_XS, t, err ? G_BG : mid);
        }
        if (set.fmt == 0)
            return;
        x += 9 * CW;
    }
    {
        int ch = ev_human(e, t, set.midc);
        if (ch) {
            char c[4];
            s_dec(c, ch, 2);
            cv_text(x, y, &FONT_XS, c, err ? G_BG : G_DIM);
        }
        x += 3 * CW;
        cv_text(x, y, &FONT_XS, t, err ? G_BG : hi);
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
static void draw_filter(void)
{
    uint32_t i;
    char t[8];
    cv_text(4, 2, &FONT_XS, "CHANNELS  (WHITE KEYS)", G_MID);
    for (i = 0; i < 16u; i++) {        /* a cell a channel, lit when shown */
        int32_t x = 4 + (int32_t)(i % 8u) * 29, y = 18 + (int32_t)(i / 8u) * 18, on = set.chans >> i & 1u;
        s_dec(t, (int32_t)i + 1, 0);
        cv_rect(x, y, 26, 15, on ? G_MID : G_FAINT);
        cv_text(x + (26 - text_w(&FONT_XS, t)) / 2, y + 2, &FONT_XS, t, on ? G_BG : G_DIM);
    }
    cv_text(4, 60, &FONT_XS, "MESSAGES  (BLACK KEYS)", G_MID);
    for (i = 0; i < NTY; i++) {
        int32_t x = 4 + (int32_t)(i / 6u) * 118, y = 76 + (int32_t)(i % 6u) * 14, on = set.types >> i & 1u;
        cv_text(x, y, &FONT_XS, on ? "[X]" : "[ ]", on ? G_HI : G_DIM);
        cv_text(x + 4 * CW, y, &FONT_XS, TY_NAME[i], on ? G_HI : G_DIM);
    }
    cv_text(4, 164, &FONT_XS, "SOURCES  FX USB SEL TRS ENV BLE LFO DEV", G_DIM);
    cv_text(4, 178, &FONT_XS, "OCT- NONE  OCT+ ALL  EDIT DEFAULTS", G_DIM);
}

/* Bluetooth's state, three lines from y; returns the y after them */
static int32_t draw_bt_state(int32_t y)
{
    static const char *const DEV[] = {"--", "SCANNING", "CONNECTING", "SEARCHING", "CONNECTED"};
    int st = plat_ble_device();
    cv_text(4, y, &FONT_XS, "BLUETOOTH", G_MID);
    cv_text(4 + 11 * CW, y, &FONT_XS, !FM1B_BLE ? "NOT BUILT" : set.ble_off ? "OFF" :
            plat_ble_state() == BLE_READY ? "ON" : plat_ble_state() == BLE_FAILED ? "FAILED" : "STARTING", G_HI);
    y += 14;
    cv_text(4, y, &FONT_XS, " COMPUTER", G_DIM);
    cv_text(4 + 11 * CW, y, &FONT_XS, plat_ble_connected() ? "CONNECTED" : plat_ble_linked() ? "LINKED" : "--", G_HI);
    y += 14;
    cv_text(4, y, &FONT_XS, " DEVICE", G_DIM);
    cv_text(4 + 11 * CW, y, &FONT_XS, st >= 0 && st <= BLE_DEV_READY ? DEV[st] : "?", G_HI);
    if (*btdev_name() && st != BLE_DEV_OFF && st != BLE_DEV_SCANNING)
        cv_text(4 + 22 * CW, y, &FONT_XS, btdev_name(), G_MID);
    return y + 14;
}

/* ------------------------------------------------------------------- GLO --- */
static void draw_glo(void)
{
    static const char *const LABEL[4] = {"K1 BLUETOOTH", "K2 DEVICE", "K3 THRU", "K4 NOTE 60"};
    char name[21];
    const char *val[4];
    int32_t y = 2, i, n;
    val[0] = !FM1B_BLE ? "NOT BUILT" : set.ble_off ? "OFF" : "ON";
    val[1] = !FM1B_BLE_CENTRAL || set.ble_off ? "--" : btdev_busy() ? btdev_name() : btdev_count() ? "TURN TO PICK"
             : "SCANNING..";
    val[2] = set.thru ? "ON  (ALL IN -> OUT)" : "OFF";
    val[3] = set.midc == 3u ? "C3 (YAMAHA)" : "C4 (ROLAND)";
    for (i = 0; i < 4; i++, y += 15) {
        cv_text(4, y, &FONT_XS, LABEL[i], G_MID);
        cv_text(4 + 14 * CW, y, &FONT_XS, val[i], G_HI);
    }
    y += 4;
    cv_rect(4, y, 232, 1, G_FAINT);
    y += 6;
    y = draw_bt_state(y);
    y += 6;
    if (FM1B_BLE_CENTRAL && !set.ble_off && !btdev_busy()) {   /* the devices found, the pick lit */
        cv_text(4, y, &FONT_XS, "MIDI DEVICES AROUND", G_MID);
        y += 14;
        n = btdev_count();
        for (i = 0; i < n && y < BODY_H - LINE_H; i++, y += 13) {
            int rssi = 0;
            btdev_found(i, name, &rssi);
            if (i == btdev_pick() && ui.ask == ASK_CONNECT)
                text_inv(10, y, &FONT_XS, name, G_HI);
            else
                cv_text(10, y, &FONT_XS, name, G_HI);
        }
        if (!n)
            cv_text(10, y, &FONT_XS, (plat_ms() / 500u) & 1u ? "SCANNING_" : "SCANNING", G_DIM);
    }
}

/* ----------------------------------------------------------------- STATS --- */
static void draw_stats(void)
{
    uint32_t s;
    char t[48], *d;
    int32_t y = 2;
    cv_text(4, y, &FONT_XS, "SRC   EVENTS   /S   BYTES  LOST", G_MID);
    for (s = 0; s < NTAPS; s++) {
        y += 14;
        d = s_cat(t, SRC_NAME[s]);
        d = s_dec(d, (int32_t)src_events[s], 9);
        d = s_dec(d, (int32_t)ui.rate[s], 5);
        d = s_dec(d, (int32_t)plat_tap_bytes((int)s), 8);
        s_dec(d, (int32_t)src_lost_seen[s], 6);
        cv_text(4, y, &FONT_XS, t, set.srcs >> s & 1u ? G_HI : G_DIM);
    }
    y += 22;
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
    if (ui.tab == TAB_LOG)
        draw_log();
    else if (ui.tab == TAB_FILTER)
        draw_filter();
    else if (ui.tab == TAB_STATS)
        draw_stats();
    else
        draw_glo();
    if (ui.ask) {                      /* a question: OCT- NO on the left, OCT+ YES on the right */
        char q[32] = "", name[21] = "";
        if (ui.ask == ASK_CLEAR)
            s_cat(q, "CLEAR THE LOG?");
        else if (ui.ask == ASK_BT)
            s_cat(q, set.ble_off ? "BLUETOOTH ON?" : "BLUETOOTH OFF?");
        else if (ui.ask == ASK_LET_GO)
            s_cat(q, "DISCONNECT?");
        else
            s_cat(q, "CONNECT TO");
        if (ui.ask == ASK_CONNECT)
            btdev_found(btdev_pick(), name, 0);
        else if (ui.ask == ASK_LET_GO)
            s_cat(name, btdev_name());
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
