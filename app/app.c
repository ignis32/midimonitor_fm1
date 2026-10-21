/* SPDX-License-Identifier: MPL-2.0 */
/* MIDIMON: a MIDI monitor. Every message from USB, the TRS jack and Bluetooth (a computer connected to the FM-1, and
 * the device the FM-1 connected to) in a log on the screen, in hex, in words or both, filtered by source, message
 * and channel; the lights show what comes in. Part of the unity build (device/unity.c, host/plat_host.c), after the
 * platform: gfx.c and announce.c are in scope.
 *
 * The panel:
 *   PRESETS          the page: LOG, FILTER, STATS
 *   SELECT           scroll the log back (the view stops; back to the end: live again)
 *   PLAY             pause / go on (the log keeps everything that comes meanwhile)
 *   FX SEL ENV LFO   show / hide USB, TRS, BLE (the computer), DEV (the Bluetooth device)
 *   K1 K2 K3 K4      HEX / HUMAN / BOTH; the time (+ms, since power-on, none); C3 / C4 for note 60; the scanlines
 *   HOME             clear the log (asks: OCT+ yes, OCT- no)
 *   SAVE             keep the settings (lit: changed)
 *   REC              a Bluetooth MIDI device: scan, ALGORITHM picks, REC again connects; held a second, Bluetooth
 *                    off / on
 *   FILTER page      white keys the channels, black keys the messages; OCT- none, OCT+ all, EDIT the defaults
 * The lights: the source buttons flash with what comes in (dim: hidden); the keys light with the notes held (F3..G5,
 * OCT- / OCT+: some below / above); PLAY green on the beat of a clock coming in, red while paused or on an error. */

#define SET_MAGIC 0x4D4F4E31u            /* "MON1": the settings record; another magic loads defaults */
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
    uint8_t rsv[2];
} settings_t;
static settings_t set;
static uint8_t set_dirty;

static struct {
    uint32_t tab;
    uint8_t paused, ask, looking, redraw_all;
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
    uint32_t keys, btn, rec_t;
} ui;

#include "mon.c"
#include "view.c"

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
    filters_default();
}

void app_init(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.fmt > 2u || set.tmode > 2u || (set.midc != 3u && set.midc != 4u) || set.ble_off > 1u)
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

static void ble_toggle(void)             /* REC held a second: Bluetooth off or on (kept with the settings) */
{
    set.ble_off = (uint8_t)!set.ble_off;
    plat_ble_enable(!set.ble_off);
    changed();
    app_say("BLUETOOTH", set.ble_off ? "OFF" : "ON");
}
static void ble_device_say(void)
{
    char name[21];
    if (btdev_found(btdev_pick(), name, 0))
        app_say("DEVICE", name);
    else
        app_say("BLE", "SCANNING");
}
static void ble_device_button(void)      /* REC: look for devices; again: connect to the one ALGORITHM picked */
{
    int ev;
    if (!btdev_busy() && !ui.looking) {
        ui.looking = 1;
        app_say("BLE", "SCANNING");
        return;
    }
    ev = btdev_save();
    ui.looking = 0;
    app_say(ev == BTDEV_CONNECTING ? btdev_word(ev) : "BLE", ev == BTDEV_CONNECTING ? btdev_name() : btdev_word(ev));
}

static void button(uint32_t k)
{
    uint32_t s;
    if (ui.ask) {                        /* a question: OCT+ yes, OCT- no, nothing else */
        if (k == B_OCTUP) {
            mon_clear();
            ui.scroll = 0;
            ui.head = 0;
            for (s = 0; s < 128u; s++)
                ui.held[s] = 0;
            app_say("LOG", "CLEARED");
        }
        if (k == B_OCTUP || k == B_OCTDN) {
            ui.ask = 0;
            ui.redraw_all = 1;
        }
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
        ui.ask = 1;
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
    case B_REC:
        if (FM1B_BLE)
            ui.rec_t = plat_ms() | 1u;   /* on release (held: Bluetooth off / on) */
        break;
    case B_EDIT:
        if (ui.tab == TAB_FILTER) {
            set.types = (uint16_t)TYPES_DEFAULT;
            set.chans = 0xFFFFu;
            changed();
            app_say("FILTERS", "DEFAULT");
        }
        break;
    case B_OCTDN:
    case B_OCTUP:
        if (ui.tab == TAB_FILTER) {
            set.types = k == B_OCTUP ? (uint16_t)((1u << NTY) - 1u) : 0u;
            set.chans = k == B_OCTUP ? 0xFFFFu : 0u;
            changed();
            app_say("FILTERS", k == B_OCTUP ? "ALL" : "NONE");
        }
        break;
    }
}

static void key(uint32_t k)              /* a key pressed: on the FILTER page, a channel or a message on / off */
{
    uint32_t i;
    if (ui.tab != TAB_FILTER || ui.ask)
        return;
    for (i = 0; i < 16u; i++)
        if (WHITE_K[i] == k) {
            char t[4];
            set.chans ^= (uint16_t)(1u << i);
            changed();
            s_dec(t, (int32_t)i + 1, 0);
            app_say(set.chans >> i & 1u ? "SHOW CH" : "HIDE CH", t);
            return;
        }
    for (i = 0; i < NTY; i++)
        if (BLACK_K[i] == k) {
            set.types ^= (uint16_t)(1u << i);
            changed();
            app_say(set.types >> i & 1u ? "SHOW" : "HIDE", TY_NAME[i]);
            return;
        }
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), k;
    int32_t e;
    for (k = 0; k < NKEYS; k++)
        if ((keys >> k & 1u) && !(ui.keys >> k & 1u))
            key(k);
    for (k = 0; k < NB; k++)
        if ((btn >> k & 1u) && !(ui.btn >> k & 1u))
            button(k);
    if (FM1B_BLE && ui.rec_t) {         /* REC: held a second, Bluetooth on / off; a press, a device */
        if (btn >> B_REC & 1u) {
            if (plat_ms() - ui.rec_t >= 1000u) {
                ui.rec_t = 0;
                ble_toggle();
            }
        } else {
            ui.rec_t = 0;
            if (FM1B_BLE_CENTRAL && !set.ble_off)
                ble_device_button();
        }
    }
    ui.keys = keys;
    ui.btn = btn;
    if ((e = plat_enc(EN_PRESET)) != 0 && !ui.ask) {
        ui.tab = (uint32_t)((int32_t)ui.tab + (e > 0 ? 1 : -1) + NTABS) % NTABS;
        ui.redraw_all = 1;
    }
    if ((e = plat_enc(EN_SELECT)) != 0 && ui.tab == TAB_LOG && !ui.ask)
        scroll_by(-e);
    if ((e = plat_enc(EN_ALGO)) != 0 && FM1B_BLE_CENTRAL && ui.looking) {
        btdev_choose(btdev_pick() + e);
        ble_device_say();
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
        set.midc = set.midc == 3u ? 4u : 3u;
        changed();
        app_say("NOTE 60 IS", set.midc == 3u ? "C3" : "C4");
    }
    if (plat_enc(EN_K4) != 0) {
        set.scan = (uint8_t)!set.scan;
        changed();
        app_say("SCANLINES", set.scan ? "ON" : "OFF");
    }
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
    if (ui.tab == TAB_FILTER) {          /* the keys: the channels and messages shown */
        for (n = 0; n < 16u; n++)
            if (set.chans >> n & 1u)
                bk |= 1u << WHITE_K[n];
            else
                dk |= 1u << WHITE_K[n];
        for (n = 0; n < NTY; n++)
            if (set.types >> n & 1u)
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
    if (FM1B_BLE_CENTRAL) {              /* looking: a device found is named at once (REC connects to it) */
        static int found;
        int n = ui.looking ? btdev_count() : 0, ev;
        btdev_page(ui.looking && !set.ble_off);
        if (n > found && btdev_pick() < n)
            ble_device_say();
        found = n;
        while ((ev = btdev_event()) != BTDEV_NONE)
            if (ev == BTDEV_CONNECTED || ev == BTDEV_NO_ANSWER || ev == BTDEV_LOST)
                app_say("DEVICE", btdev_word(ev));
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
           (uint32_t)set.scan << 15;
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw_all = 1;
    }
    if (now - last_slow >= 500u) {       /* the blinking, the STATS figures */
        last_slow = now;
        ui.redraw_all = 1;
    }
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
    else
        return 0;
    return 1;
}
#endif
