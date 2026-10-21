/* SPDX-License-Identifier: MPL-2.0 */
/* MIDIMON: a MIDI monitor. Every message from USB, the TRS jack and Bluetooth (a computer connected to the FM-1, and
 * the device the FM-1 connected to) in a log on the screen, in hex, in words or both, filtered by source, message
 * and channel; the lights show what comes in. Part of the unity build (device/unity.c, host/plat_host.c), after the
 * platform: gfx.c and announce.c are in scope.
 *
 * The panel:
 *   PRESETS          the page: LOG, FILTER, STATS, THRU, GLO
 *   GLO              the settings page, and back
 *   SELECT           scroll the log back (the view stops; back to the end: live again)
 *   PLAY             pause / go on (the log keeps everything that comes meanwhile)
 *   FX SEL ENV LFO   show / hide USB, TRS, BLE (the computer), DEV (the Bluetooth device)
 *   K1 K2 K3         HEX / HUMAN / BOTH; the time (+ms, since power-on, none); the scanlines
 *   HOME             clear the log (asks: OCT+ yes, OCT- no)
 *   SAVE             keep the settings (lit: changed)
 *   FILTER page      white keys the channels, black keys the messages; OCT- none, OCT+ all, EDIT the defaults
 *   THRU page        SELECT a source; K1 K2 K3 its route to USB, BLE-P, BLE-C (any of them, not back to itself);
 *                    K4 thru on / off (the routes kept)
 *   GLO page         K1 Bluetooth on / off (asks), K2 a Bluetooth device (scans while the page is up; turn: asks to
 *                    connect to the one picked, or to let the connected one go), K3 C3 / C4 for note 60
 * The lights: the source buttons flash with what comes in (dim: hidden); the keys light with the notes held (F3..G5,
 * OCT- / OCT+: some below / above); PLAY green on the beat of a clock coming in, red while paused or on an error. */

#define SET_MAGIC 0x4D4F4E33u            /* "MON3": the settings record; another magic loads defaults */
/* thru's outputs, a bit each (settings_t routes) */
enum { OUT_USB, OUT_BLE_P, OUT_BLE_C, NOUTS };
static const uint32_t OUT_TO[NOUTS] = {TO_USB, TO_BLE_P, TO_BLE_C};
static const uint8_t SRC_SELF[NTAPS] = {1u << OUT_USB, 0, 1u << OUT_BLE_P, 1u << OUT_BLE_C};   /* not back to itself */
typedef struct {
    uint32_t magic;
    uint8_t fmt;                         /* 0 HEX, 1 HUMAN, 2 BOTH */
    uint8_t tmode;                       /* 0 ms since the line above, 1 since power-on, 2 none */
    uint8_t midc;                        /* 3 or 4: the octave of note 60 */
    uint8_t scan;                        /* scanlines */
    uint8_t srcs;                        /* bit TAP_*: the sources shown */
    uint8_t ble_off;                     /* Bluetooth switched off (REC held) */
    uint16_t types;                      /* bit TY_*: the messages shown */
    uint16_t chans;                      /* bit c: channel c + 1 shown */
    uint8_t thru;                        /* thru on: what comes in goes out again by routes (thru()) */
    uint8_t routes[NTAPS];               /* per source: bit OUT_* = it goes out there */
    uint8_t rsv[3];
    uint16_t rt_types[NTAPS][NOUTS];     /* each route's filter: bit TY_* passes (as the display's) */
    uint16_t rt_chans[NTAPS][NOUTS];     /* bit c: channel c + 1 passes */
} settings_t;
static settings_t set;
static uint8_t set_dirty;

/* the questions (OCT+ yes, OCT- no) */
enum { ASK_NONE, ASK_CLEAR, ASK_BT, ASK_CONNECT, ASK_LET_GO };

static struct {
    uint32_t tab, tab_before_glo;
    uint8_t paused, ask, redraw_all;
    uint32_t scroll, head;               /* lines back from the end; the end the view stays at while it stops */
    uint32_t act[NTAPS];                 /* when each source last sent something (not clock / active sensing) */
    uint32_t rate[NTAPS], rate_from[NTAPS], rate_t;   /* events a second */
    uint32_t err_ms;                     /* the last error (lost, cut, stray) */
    uint8_t held[128], vel[128];         /* notes held (all sources shown), their last velocity */
    uint32_t clk_ms, clk_n, beat_ms, clk_src, bpm10;   /* clock in: the last F8, its count, the last beat */
    uint16_t clk_dt[24];
    uint32_t clk_i, clk_sum;
    uint8_t running;
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    uint32_t thru_row;                   /* the THRU page's source */
    uint8_t rt_edit, rt_out;             /* THRU: a route's filter up (EDIT), its output */
    uint32_t route_ms[NTAPS][NOUTS];     /* when each route last passed something on */
    uint32_t routed[NTAPS];              /* messages each source passed on */
} ui;

#include "mon.c"
static int rt_filtered(uint32_t src, uint32_t o);
#include "view.c"

/* the filter being edited by the keys: the display's (FILTER), or a route's (THRU, EDIT) */
static int filter_up(void) { return ui.tab == TAB_FILTER || (ui.tab == TAB_THRU && ui.rt_edit); }
static uint16_t *filter_types(void)
{
    return ui.tab == TAB_THRU ? &set.rt_types[ui.thru_row][ui.rt_out] : &set.types;
}
static uint16_t *filter_chans(void)
{
    return ui.tab == TAB_THRU ? &set.rt_chans[ui.thru_row][ui.rt_out] : &set.chans;
}

static const uint8_t SRC_BTN[NTAPS] = {B_FX, B_SEL, B_ENV, B_LFO};
static const uint8_t WHITE_K[16] = PLAT_WHITE_KEYS, BLACK_K[11] = PLAT_BLACK_KEYS;

/* ------------------------------------------------------------- an event --- */
static void clock_in(const ev_t *e)
{
    uint32_t st = e->b[0];
    if (st == 0xF8u) {
        if (ui.clk_ms && e->ms - ui.clk_ms < 500u) {   /* tempo: the last 24 ticks, a beat */
            uint32_t dt = e->ms - ui.clk_ms;
            ui.clk_sum += dt - ui.clk_dt[ui.clk_i % 24u];
            ui.clk_dt[ui.clk_i % 24u] = (uint16_t)dt;
            ui.clk_i++;
            if (ui.clk_sum)
                ui.bpm10 = ui.clk_i >= 24u ? 600000u / ui.clk_sum : 600000u * ui.clk_i / 24u / ui.clk_sum;
        } else {
            ui.clk_i = ui.clk_sum = 0;
            for (st = 0; st < 24u; st++)
                ui.clk_dt[st] = 0;
        }
        if (ui.clk_n++ % 24u == 0)
            ui.beat_ms = e->ms;
        ui.clk_ms = e->ms;
        ui.clk_src = e->src;
    } else if (st == 0xFAu) {
        ui.clk_n = 0;
        ui.running = 1;
    } else if (st == 0xFBu) {
        ui.running = 1;
    } else if (st == 0xFCu) {
        ui.running = 0;
    }
}

static void mon_event(const ev_t *e)     /* each event as it is logged (mon.c) */
{
    uint32_t st = e->b[0];
    if (e->type == TY_CLOCK || e->type == TY_TRANSPORT) {
        clock_in(e);
        if (e->type == TY_CLOCK)
            return;
    }
    if (e->type == TY_ERR)
        ui.err_ms = plat_ms() | 1u;
    if (e->type != TY_ACTIVE)
        ui.act[e->src] = plat_ms() | 1u;
    if (e->type == TY_NOTE && ev_shown(e)) {   /* the notes held, for the keys' lights */
        uint32_t n = e->b[1];
        if ((st & 0xF0u) == 0x90u && e->b[2]) {
            if (ui.held[n] < 255u)
                ui.held[n]++;
            ui.vel[n] = e->b[2];
        } else if (ui.held[n]) {
            ui.held[n]--;
        }
    } else if (e->type == TY_CC && (e->b[1] == 123u || e->b[1] == 120u)) {   /* all notes / sound off */
        uint32_t n;
        for (n = 0; n < 128u; n++)
            ui.held[n] = 0;
    }
}

/* --------------------------------------------------------------- thru --- */
/* THRU (its page): each message out again as it comes, by its source's routes (one source to any of USB, BLE-P,
 * BLE-C; never back to itself). SysEx to USB only: the Bluetooth side sends no SysEx yet. Cut-off and stray bytes
 * are not passed on */
static struct {
    uint8_t n, b[3];
} thru_sx[NTAPS];

#define RT_ALL_TYPES ((1u << NTY) - 1u)
static int rt_filtered(uint32_t src, uint32_t o)   /* the route lets some messages by, not all */
{
    return set.rt_types[src][o] != RT_ALL_TYPES || set.rt_chans[src][o] != 0xFFFFu;
}

/* TO_* of a source's routes that let this message by (type TY_*, channel 0..15 or -1), each marked as used now */
static uint32_t thru_to(uint32_t src, uint32_t type, int ch)
{
    uint32_t o, to = 0, r = set.routes[src] & ~SRC_SELF[src];
    for (o = 0; o < NOUTS; o++)
        if ((r >> o & 1u) && (set.rt_types[src][o] >> type & 1u) && (ch < 0 || (set.rt_chans[src][o] >> ch & 1u))) {
            to |= OUT_TO[o];
            ui.route_ms[src][o] = plat_ms() | 1u;
        }
    if (to)
        ui.routed[src]++;
    return to;
}

static void thru(uint32_t w)             /* each tap entry (mon_poll) */
{
    uint32_t src = TAP_SRC(w), kind = TAP_KIND(w), n = TAP_LEN(w), b0 = TAP_B(w, 0), i, cin;
    if (!set.thru)
        return;
    if (kind == TK_SX) {                 /* SysEx in USB-MIDI's packets: 3 bytes a packet, the last says how many */
        if (!(set.routes[src] & ~SRC_SELF[src] & 1u << OUT_USB) || !(set.rt_types[src][OUT_USB] >> TY_SYSEX & 1u))
            return;
        for (i = 0; i < n; i++) {
            uint32_t b = TAP_B(w, i), k;
            if (b == 0xF0u)
                thru_sx[src].n = 0;
            k = thru_sx[src].n++;
            thru_sx[src].b[k] = (uint8_t)b;
            if (b == 0xF7u || k == 2u) {
                if (b == 0xF7u) {        /* a whole SysEx passed on: counted, lit */
                    ui.route_ms[src][OUT_USB] = plat_ms() | 1u;
                    ui.routed[src]++;
                }
                plat_midi_send((b == 0xF7u ? 5u + k : 4u) | (uint32_t)thru_sx[src].b[0] << 8 |
                               (k > 0u ? (uint32_t)thru_sx[src].b[1] << 16 : 0u) |
                               (k > 1u ? (uint32_t)thru_sx[src].b[2] << 24 : 0u), TO_USB);
                thru_sx[src].n = 0;
            }
        }
        return;
    }
    if (kind == TK_CUT && b0 == 0xF0u)
        thru_sx[src].n = 0;              /* a SysEx cut off: what it still owed is dropped */
    if (kind != TK_MSG && kind != TK_RS)
        return;
    cin = b0 >= 0xF8u ? 0xFu : b0 < 0xF0u ? b0 >> 4 : n == 1u ? 5u : n;   /* system common: 1, 2, 3 bytes */
    if (set.routes[src] & ~SRC_SELF[src]) {
        uint32_t to = thru_to(src, ev_type(b0), b0 < 0xF0u ? (int)(b0 & 15u) : -1);
        if (to)
            plat_midi_send(cin | (w & 0xFFFFFFu) << 8, to);
    }
}

/* -------------------------------------------------------------- audio --- */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, pkt;
    (void)master_q12;
    while (plat_midi_in(&pkt, 0))        /* (the instrument's queue: the log reads the tap) */
        ;
    for (i = 0; i < 2u * n; i++)
        lr[i] = 0;
}

void app_panic(void) {}

/* ----------------------------------------------------------- settings --- */
#define TYPES_DEFAULT (((1u << NTY) - 1u) & ~(1u << TY_CLOCK) & ~(1u << TY_ACTIVE))   /* (as MIDI-OX: no clock) */
static void filters_default(void)
{
    set.srcs = (1u << NTAPS) - 1u;
    set.types = (uint16_t)TYPES_DEFAULT;
    set.chans = 0xFFFFu;
}
static void settings_defaults(void)
{
    set = (settings_t){0};
    set.magic = SET_MAGIC;
    set.fmt = 2;
    set.midc = 4;
    set.scan = 1;
    set.routes[TAP_TRS] = 1u << OUT_USB;      /* a keyboard on the jack to the computer */
    set.routes[TAP_BLE_DEV] = 1u << OUT_USB;  /* a Bluetooth device to the computer (as a WIDI does) */
    {
        uint32_t s, o;
        for (s = 0; s < NTAPS; s++)
            for (o = 0; o < NOUTS; o++) {       /* a route lets everything by, clock too */
                set.rt_types[s][o] = (uint16_t)RT_ALL_TYPES;
                set.rt_chans[s][o] = 0xFFFFu;
            }
    }
    filters_default();
}

void app_init(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.fmt > 2u || set.tmode > 2u || (set.midc != 3u && set.midc != 4u) || set.ble_off > 1u || set.thru > 1u)
        settings_defaults();
}

int mon_ble_off(void) { return set.ble_off; }

/* ------------------------------------------------------------- words --- */
void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[52], *d;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw_all = 1;
    if (ann_on()) {                      /* and to a screen reader (announce.c, fm1-reader) */
        d = s_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            *d++ = ' ';
            s_cat(d, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void speak(void)                  /* a screen reader that has just started listening hears the page */
{
    char t[64], *d;
    if (ann_on() && ann_where()) {
        d = s_cat(t, APP_NAME ". ");
        d = s_cat(d, TAB_NAME[ui.tab]);
        if (ui.paused)
            s_cat(d, ". PAUSED");
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

/* ------------------------------------------------------------- input --- */
static void changed(void)
{
    set_dirty = 1;
    ui.redraw_all = 1;
}

static uint32_t head_now(void) { return ui.paused || ui.scroll ? ui.head : ev_w; }

static void scroll_by(int32_t d)         /* + older, - newer */
{
    int32_t idx[2];
    if (!ui.paused && !ui.scroll)
        ui.head = ev_w;                  /* the view stops where it is */
    if (d < 0)
        ui.scroll = (uint32_t)-d >= ui.scroll ? 0u : ui.scroll + (uint32_t)d;
    else
        ui.scroll += (uint32_t)d;
    while (ui.scroll && !log_collect(ui.head, ui.scroll + NLINES - 1u, idx, 1))   /* not past the oldest */
        ui.scroll--;
    ui.redraw_all = 1;
}

/* GLO: Bluetooth on / off (K1, asks first), a Bluetooth device (K2: picks one of those found, asks to connect;
 * connected, asks to let it go) */
static void glo_bt_knob(void)
{
    if (FM1B_BLE)
        ui.ask = ASK_BT;
    ui.redraw_all = 1;
}
static void glo_device_knob(int32_t e)
{
    if (!FM1B_BLE_CENTRAL || set.ble_off)
        return;
    if (btdev_busy()) {
        ui.ask = ASK_LET_GO;
    } else if (btdev_count()) {
        if (ui.ask == ASK_CONNECT)       /* the question up: the knob moves the pick */
            btdev_choose(btdev_pick() + e);
        ui.ask = ASK_CONNECT;
    } else {
        app_say("NO DEVICES", "FOUND YET");
    }
    ui.redraw_all = 1;
}

static void answer(int yes)              /* the question's answer */
{
    uint32_t s;
    int ev;
    if (yes && ui.ask == ASK_CLEAR) {
        mon_clear();
        ui.scroll = 0;
        ui.head = 0;
        for (s = 0; s < 128u; s++)
            ui.held[s] = 0;
        app_say("LOG", "CLEARED");
    } else if (yes && ui.ask == ASK_BT) {   /* kept with the settings: as it is at the next power-on too */
        set.ble_off = (uint8_t)!set.ble_off;
        plat_ble_enable(!set.ble_off);
        plat_store_save(OBJ_SETTINGS, &set, sizeof set);
        app_say("BLUETOOTH", set.ble_off ? "OFF" : "ON");
    } else if (yes && (ui.ask == ASK_CONNECT || ui.ask == ASK_LET_GO)) {
        ev = btdev_save();
        app_say(btdev_word(ev), ev == BTDEV_CONNECTING ? btdev_name() : "");
    }
    ui.ask = ASK_NONE;
    ui.redraw_all = 1;
}

static void button(uint32_t k)
{
    uint32_t s;
    if (ui.ask) {                        /* a question: OCT+ yes, OCT- no, nothing else */
        if (k == B_OCTUP || k == B_OCTDN)
            answer(k == B_OCTUP);
        return;
    }
    if (k == B_GLO) {                    /* the settings page, and back */
        if (ui.tab == TAB_GLO) {
            ui.tab = ui.tab_before_glo;
        } else {
            ui.tab_before_glo = ui.tab;
            ui.tab = TAB_GLO;
        }
        ui.redraw_all = 1;
        return;
    }
    for (s = 0; s < NTAPS; s++)
        if (k == SRC_BTN[s]) {
            set.srcs ^= (uint8_t)(1u << s);
            changed();
            app_say(SRC_NAME[s], set.srcs >> s & 1u ? "SHOWN" : "HIDDEN");
            return;
        }
    switch (k) {
    case B_PLAY:
        ui.paused = !ui.paused;
        if (ui.paused && !ui.scroll)
            ui.head = ev_w;
        if (!ui.paused)
            ui.scroll = 0;
        ui.redraw_all = 1;
        break;
    case B_HOME:
        ui.ask = ASK_CLEAR;
        ui.redraw_all = 1;
        break;
    case B_SAVE:
        if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) {
            set_dirty = 0;
            app_say("SAVED", "");
        } else {
            app_say("SAVE", "FAILED");
        }
        break;
    case B_EDIT:
        if (ui.tab == TAB_THRU) {        /* a route's filter, and back to the routes */
            ui.rt_edit = !ui.rt_edit;
            ui.redraw_all = 1;
        } else if (ui.tab == TAB_FILTER) {
            set.types = (uint16_t)TYPES_DEFAULT;
            set.chans = 0xFFFFu;
            changed();
            app_say("FILTERS", "DEFAULT");
        }
        break;
    case B_OCTDN:
    case B_OCTUP:
        if (filter_up()) {               /* all through, or nothing */
            *filter_types() = k == B_OCTUP ? (uint16_t)((1u << NTY) - 1u) : 0u;
            *filter_chans() = k == B_OCTUP ? 0xFFFFu : 0u;
            changed();
            app_say("FILTER", k == B_OCTUP ? "ALL" : "NONE");
        }
        break;
    }
}

static void key(uint32_t k)              /* a key pressed: with a filter up, a channel or a message on / off */
{
    uint32_t i;
    uint16_t *types = filter_types(), *chans = filter_chans();
    if (!filter_up() || ui.ask)
        return;
    for (i = 0; i < 16u; i++)
        if (WHITE_K[i] == k) {
            char t[4];
            *chans ^= (uint16_t)(1u << i);
            changed();
            s_dec(t, (int32_t)i + 1, 0);
            app_say(*chans >> i & 1u ? "CH ON" : "CH OFF", t);
            return;
        }
    for (i = 0; i < NTY; i++)
        if (BLACK_K[i] == k) {
            *types ^= (uint16_t)(1u << i);
            changed();
            app_say(*types >> i & 1u ? "ON" : "OFF", TY_NAME[i]);
            return;
        }
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), k;
    int32_t e, sel;
    for (k = 0; k < NKEYS; k++)
        if ((keys >> k & 1u) && !(ui.keys >> k & 1u))
            key(k);
    for (k = 0; k < NB; k++)
        if ((btn >> k & 1u) && !(ui.btn >> k & 1u))
            button(k);
    ui.keys = keys;
    ui.btn = btn;
    if ((e = plat_enc(EN_PRESET)) != 0 && !ui.ask) {
        ui.tab = (uint32_t)((int32_t)ui.tab + (e > 0 ? 1 : -1) + NTABS) % NTABS;
        ui.redraw_all = 1;
    }
    sel = plat_enc(EN_SELECT);           /* (read once: the LOG scrolls with it, THRU picks a source) */
    if (sel && ui.tab == TAB_LOG && !ui.ask)
        scroll_by(-sel);
    if (ui.tab == TAB_GLO) {             /* the settings: Bluetooth, the device, note 60, the scanlines */
        if (plat_enc(EN_K1) != 0 && !ui.ask)
            glo_bt_knob();
        if ((e = plat_enc(EN_K2)) != 0 && (!ui.ask || ui.ask == ASK_CONNECT))
            glo_device_knob(e);
        if (plat_enc(EN_K3) != 0 && !ui.ask) {
            set.midc = set.midc == 3u ? 4u : 3u;
            changed();
        }
        plat_enc(EN_K4);                 /* (free for now) */
        return;
    }
    if (ui.tab == TAB_THRU) {            /* SELECT the source; K1 K2 K3 its route to USB, BLE-P, BLE-C; K4 thru on */
        static const uint8_t OUT_ENC[NOUTS] = {EN_K1, EN_K2, EN_K3};
        uint32_t o;
        if (sel) {
            ui.thru_row = (uint32_t)(((int32_t)ui.thru_row + sel % NTAPS + NTAPS) % NTAPS);
            ui.redraw_all = 1;
        }
        for (o = 0; o < NOUTS; o++)
            if (plat_enc(OUT_ENC[o]) != 0) {
                if (ui.rt_edit) {        /* a filter up: the knob picks the output whose filter it is */
                    ui.rt_out = (uint8_t)o;
                    ui.redraw_all = 1;
                } else if (!(SRC_SELF[ui.thru_row] >> o & 1u)) {
                    set.routes[ui.thru_row] ^= (uint8_t)(1u << o);
                    changed();
                }
            }
        if (plat_enc(EN_K4) != 0) {
            set.thru = (uint8_t)!set.thru;
            changed();
            app_say("THRU", set.thru ? "ON" : "OFF");
        }
        return;
    }
    if ((e = plat_enc(EN_K1)) != 0) {
        set.fmt = (uint8_t)(((int32_t)set.fmt + (e > 0 ? 1 : 2)) % 3);
        changed();
        app_say("FORMAT", FMT_NAME[set.fmt]);
    }
    if ((e = plat_enc(EN_K2)) != 0) {
        static const char *const T[3] = {"+MS", "SINCE START", "NONE"};
        set.tmode = (uint8_t)(((int32_t)set.tmode + (e > 0 ? 1 : 2)) % 3);
        changed();
        app_say("TIME", T[set.tmode]);
    }
    if (plat_enc(EN_K3) != 0) {
        set.scan = (uint8_t)!set.scan;
        changed();
        app_say("SCANLINES", set.scan ? "ON" : "OFF");
    }
    plat_enc(EN_K4);                     /* (free for now) */
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------ lights --- */
static void lights(void)
{
    static uint32_t last[6] = {1};
    uint32_t bb = 0, bk = 0, db = 0, dk = 0, mb = 0, mk = 0, s, n, now = plat_ms();
    for (s = 0; s < NTAPS; s++) {        /* the sources: a flash as something comes, lit when shown, dim when not */
        uint32_t b = 1u << SRC_BTN[s];
        if (!(set.srcs >> s & 1u))
            db |= b;
        else if (ui.act[s] && now - ui.act[s] < 70u)
            bb |= b;
        else
            mb |= b;
    }
    if (filter_up()) {                   /* the keys: the channels and messages the filter lets by */
        uint32_t types = *filter_types(), chans = *filter_chans();
        for (n = 0; n < 16u; n++)
            if (chans >> n & 1u)
                bk |= 1u << WHITE_K[n];
            else
                dk |= 1u << WHITE_K[n];
        for (n = 0; n < NTY; n++)
            if (types >> n & 1u)
                bk |= 1u << BLACK_K[n];
            else
                dk |= 1u << BLACK_K[n];
        bb |= 1u << B_EDIT;
        db |= 1u << B_OCTDN | 1u << B_OCTUP;
    } else {                             /* the keys: the notes held, bright when played hard */
        for (n = 0; n < 128u; n++) {
            if (!ui.held[n])
                continue;
            if (n < 53u)
                bb |= 1u << B_OCTDN;
            else if (n > 79u)
                bb |= 1u << B_OCTUP;
            else if (ui.vel[n] >= 64u)
                bk |= 1u << (n - 53u);
            else
                mk |= 1u << (n - 53u);
        }
    }
    if (ui.ask)
        bb |= 1u << B_OCTDN | 1u << B_OCTUP;
    if (ui.paused || ui.scroll || (ui.err_ms && now - ui.err_ms < 150u))
        bb |= 1u << B_PLAY;              /* red */
    if (!ui.paused && !ui.scroll) {      /* green: the beat of a clock coming in */
        if (ui.beat_ms && now - ui.clk_ms < 500u && now - ui.beat_ms < 90u)
            bb |= LED_PLAY_GREEN;
        else
            db |= LED_PLAY_GREEN;
    }
    if (set_dirty)
        bb |= 1u << B_SAVE;
    else
        db |= 1u << B_SAVE;
    db |= 1u << B_HOME | 1u << B_REC;
    if (bb != last[0] || bk != last[1] || db != last[2] || dk != last[3] || mb != last[4] || mk != last[5]) {
        last[0] = bb, last[1] = bk, last[2] = db, last[3] = dk, last[4] = mb, last[5] = mk;
        plat_lights(bb, bk, db, dk, 1);
        plat_lights_mid(mb, mk);
    }
}

/* ------------------------------------------------------------- frame --- */
void app_ui_init(void)
{
    lcd_fill(0, 0, 240, 240, G_BG);
    ui.redraw_all = 1;
}

void app_loop(void) { mon_poll(); }

void app_frame(void)
{
    static uint32_t last_ev, last_body, last_slow, foot_sig;
    uint32_t now = plat_ms(), s, sig = 0;
    mon_poll();
    input();
    speak();
    if (FM1B_BLE) {                      /* GLO up: the scan for devices; what happened, in words */
        static int found;
        int ev;
        btdev_page(ui.tab == TAB_GLO && !set.ble_off);
        if (btdev_count() != found) {
            found = btdev_count();
            ui.redraw_all = 1;
        }
        while ((ev = btdev_event()) != BTDEV_NONE) {
            app_say(ev == BTDEV_COMPUTER || ev == BTDEV_COMPUTER_GONE ? "BLE" : "DEVICE", btdev_word(ev));
            if (ui.ask == ASK_LET_GO || ui.ask == ASK_CONNECT)
                ui.ask = ASK_NONE;       /* (what it asked about changed under it) */
        }
    }
    if (now - ui.rate_t >= 1000u) {      /* events a second */
        ui.rate_t = now;
        for (s = 0; s < NTAPS; s++) {
            ui.rate[s] = src_events[s] - ui.rate_from[s];
            ui.rate_from[s] = src_events[s];
        }
    }
    lights();
    for (s = 0; s < NTAPS; s++)
        sig |= (uint32_t)(ui.act[s] && now - ui.act[s] < 90u) << s;
    sig |= (uint32_t)set.srcs << 4 | (uint32_t)set.fmt << 8 | (uint32_t)set.tmode << 10 | set.midc << 12 |
           (uint32_t)set.scan << 15 | (uint32_t)set.thru << 16 | (uint32_t)ui.paused << 17 |
           (ui.scroll < 0x3FFFu ? ui.scroll : 0x3FFFu) << 18;
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw_all = 1;
    }
    if (now - last_slow >= 500u) {       /* the blinking, the STATS figures */
        last_slow = now;
        ui.redraw_all = 1;
    }
    if (ui.tab == TAB_THRU && now - last_body >= 50u)   /* the routes' flashes */
        ui.redraw_all = 1;
    if (ui.redraw_all || sig != foot_sig) {
        foot_sig = sig;
        draw_footer();
    }
    if (ui.redraw_all || (ev_w != last_ev && now - last_body >= 33u && head_now() == ev_w)) {
        if (ui.redraw_all)
            draw_header();
        ui.redraw_all = 0;
        last_ev = ev_w;
        last_body = now;
        draw_body();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    const ev_t *last = ev_w ? ev_at(ev_w - 1u) : 0;
    int32_t idx[NLINES + 1];
    if (!strcmp(what, "events"))         /* events logged */
        *got = (int)ev_w;
    else if (!strcmp(what, "lines"))     /* lines on the LOG page as it is */
        *got = log_collect(head_now(), ui.scroll, idx, NLINES);
    else if (!strncmp(what, "last_", 5) && last) {   /* the last event: kind type src n b0 b1 b2 sxn */
        const char *f = what + 5;
        *got = !strcmp(f, "kind") ? last->kind : !strcmp(f, "type") ? last->type : !strcmp(f, "src") ? last->src :
               !strcmp(f, "n") ? last->n : !strcmp(f, "b0") ? last->b[0] : !strcmp(f, "b1") ? last->b[1] :
               !strcmp(f, "b2") ? last->b[2] : !strcmp(f, "sxn") ? last->sxn : !strcmp(f, "lost") ? (int)last->sxo : -1;
    } else if (!strcmp(what, "tab"))
        *got = (int)ui.tab;
    else if (!strcmp(what, "paused"))
        *got = ui.paused;
    else if (!strcmp(what, "scroll"))
        *got = (int)ui.scroll;
    else if (!strcmp(what, "ask"))
        *got = ui.ask;
    else if (!strcmp(what, "bpm10"))
        *got = (int)ui.bpm10;
    else if (!strcmp(what, "held")) {    /* notes held */
        int n, c = 0;
        for (n = 0; n < 128; n++)
            c += ui.held[n] != 0;
        *got = c;
    } else if (!strcmp(what, "srcs"))
        *got = set.srcs;
    else if (!strcmp(what, "types"))
        *got = set.types;
    else if (!strcmp(what, "chans"))
        *got = set.chans;
    else if (!strcmp(what, "fmt"))
        *got = set.fmt;
    else if (!strcmp(what, "midc"))
        *got = set.midc;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "thru"))
        *got = set.thru;
    else if (!strncmp(what, "rt_types", 8))   /* rt_typesSO: the filter of source S to output O */
        *got = set.rt_types[(what[8] - '0') % NTAPS][(what[9] - '0') % NOUTS];
    else if (!strncmp(what, "rt_chans", 8))
        *got = set.rt_chans[(what[8] - '0') % NTAPS][(what[9] - '0') % NOUTS];
    else if (!strcmp(what, "rt_edit"))
        *got = ui.rt_edit;
    else if (!strncmp(what, "route", 5))    /* routeN: source N's outputs (bit 0 USB, 1 BLE-P, 2 BLE-C) */
        *got = set.routes[atoi(what + 5) % NTAPS];
    else if (!strcmp(what, "thru_row"))
        *got = (int)ui.thru_row;
    else if (!strcmp(what, "ble_off"))
        *got = set.ble_off;
    else
        return 0;
    return 1;
}
#endif
