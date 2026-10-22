/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 ignis32 */
/* The LOG tab's other views (ALGO turns them): KEYS (the notes held per channel, a keyboard zoomed where they are,
 * their velocity and poly pressure), CC (pitch bend and channel pressure as graphs, the controllers last moved),
 * CHANS (each channel's activity per source, its last message) and CLOCK (tempo, transport, position, MTC). What
 * they show follows the filters, as the log does; CLOCK takes the clock whatever the filter. Part of app.c, after
 * view.c. */

static uint8_t vk_vel[16][128] PLAT_POOL;   /* the notes held per channel: their velocity (0: not held) */
static uint8_t vk_at[16][128] PLAT_POOL;    /* and their poly pressure */

#define NCCS 9                         /* the controllers last moved, by channel and number (rows stay put) */
#define HIST 116                       /* the graphs' samples (a pixel each), one every HIST_MS */
#define HIST_MS 30u
static struct vis_s {
    uint32_t dup_on, orphan_off;       /* a note-on for a note held already; a note-off for a note not held */
    uint32_t ch_ms[16];                /* when each channel last had a note */
    uint32_t zoom;                     /* the zoomed keyboard's lowest note (a C) */
    struct cc_row {
        uint8_t ch, cc, val;
        uint32_t ms;
    } cc[NCCS];
    uint32_t ncc;
    int16_t bend[16];
    uint8_t cat[16];
    uint8_t bend_ch, cat_ch;
    uint32_t bend_ms, cat_ms;
    int8_t hist_b[HIST];               /* pitch bend / 128 */
    uint8_t hist_a[HIST];
    uint32_t hist_i, hist_t;
    uint32_t src_ms[NTAPS][16];        /* CHANS: when each source last sent on each channel */
    uint32_t ch_ev[16];                /* and the channel's last message (an index into the log, + 1; 0: none) */
    uint32_t pos;                      /* CLOCK: the song position in clocks (24 a beat) */
    uint8_t armed;                     /* after a start or a song position: the next clock is that position */
    uint32_t spp, spp_ms;
    uint8_t qf[8];                     /* MTC's quarter frames, put together */
    uint8_t mtc[4], mtc_rate;          /* hh mm ss ff */
    uint32_t mtc_ms;
    uint32_t song, song_ms;
} vis;

static void views_clear(void)
{
    uint32_t c, n;
    for (c = 0; c < 16u; c++)
        for (n = 0; n < 128u; n++)
            vk_vel[c][n] = vk_at[c][n] = 0;
    vis = (struct vis_s){0};
    vis.zoom = 48;
}

static void notes_off(uint32_t c)
{
    uint32_t n;
    for (n = 0; n < 128u; n++)
        vk_vel[c][n] = vk_at[c][n] = 0;
}

/* clock, transport, song position, MTC, song select: every one, filtered or not (mon_event) */
static void views_clock(const ev_t *e)
{
    uint32_t st = e->b[0];
    if (e->kind != EV_MSG && e->kind != EV_RS)
        return;
    if (st == 0xF8u && ui.running) {
        if (vis.armed)
            vis.armed = 0;
        else
            vis.pos++;
    } else if (st == 0xFAu) {
        vis.pos = 0;
        vis.armed = 1;
    } else if (st == 0xF2u) {          /* in 16ths: 6 clocks each */
        vis.spp = e->b[1] | (uint32_t)e->b[2] << 7;
        vis.spp_ms = e->ms | 1u;
        vis.pos = vis.spp * 6u;
        vis.armed = 1;
    } else if (st == 0xF3u) {
        vis.song = e->b[1];
        vis.song_ms = e->ms | 1u;
    } else if (st == 0xF1u) {          /* the eighth quarter frame completes the time */
        uint32_t k = e->b[1] >> 4 & 7u;
        vis.qf[k] = e->b[1] & 15u;
        if (k == 7u) {
            vis.mtc[0] = (uint8_t)(vis.qf[6] | (vis.qf[7] & 1u) << 4);
            vis.mtc[1] = (uint8_t)(vis.qf[4] | vis.qf[5] << 4);
            vis.mtc[2] = (uint8_t)(vis.qf[2] | vis.qf[3] << 4);
            vis.mtc[3] = (uint8_t)(vis.qf[0] | vis.qf[1] << 4);
            vis.mtc_rate = vis.qf[7] >> 1 & 3u;
            vis.mtc_ms = e->ms | 1u;
        }
    }
}

static void cc_sort(void)              /* the controllers' rows by channel, then number */
{
    uint32_t i, j;
    for (i = 1; i < vis.ncc; i++)
        for (j = i; j > 0 && (vis.cc[j - 1u].ch << 7 | vis.cc[j - 1u].cc) > (vis.cc[j].ch << 7 | vis.cc[j].cc); j--) {
            struct cc_row t = vis.cc[j];
            vis.cc[j] = vis.cc[j - 1u];
            vis.cc[j - 1u] = t;
        }
}

/* a channel message the filters let by (mon_event) */
static void views_event(const ev_t *e, uint32_t i)
{
    uint32_t st = e->b[0], c = st & 15u, n = e->b[1], v = e->b[2], k;
    if ((e->kind != EV_MSG && e->kind != EV_RS) || st >= 0xF0u)
        return;
    vis.src_ms[e->src][c] = e->ms | 1u;
    vis.ch_ev[c] = i + 1u;
    switch (st >> 4) {
    case 0x8:
    case 0x9:                          /* (a note-on at velocity 0 is a note-off) */
        if ((st >> 4) == 0x9u && v) {
            if (vk_vel[c][n])
                vis.dup_on++;
            vk_vel[c][n] = (uint8_t)v;
        } else {
            if (!vk_vel[c][n])
                vis.orphan_off++;
            vk_vel[c][n] = 0;
        }
        vk_at[c][n] = 0;
        vis.ch_ms[c] = e->ms | 1u;
        break;
    case 0xA:
        if (vk_vel[c][n])
            vk_at[c][n] = (uint8_t)v;
        break;
    case 0xB:
        if (n == 120u || n == 123u)
            notes_off(c);
        if (n == 121u) {               /* reset all controllers */
            vis.bend[c] = 0;
            vis.cat[c] = 0;
        }
        for (k = 0; k < vis.ncc && (vis.cc[k].ch != c || vis.cc[k].cc != n); k++)
            ;
        if (k == vis.ncc) {            /* a new one: a free row, else the row untouched longest */
            if (vis.ncc < NCCS) {
                vis.ncc++;
            } else {
                uint32_t j;
                for (k = 0, j = 1; j < NCCS; j++)
                    if (vis.cc[k].ms - vis.cc[j].ms < 0x80000000u)
                        k = j;
            }
            vis.cc[k].ch = (uint8_t)c;
            vis.cc[k].cc = (uint8_t)n;
            vis.cc[k].val = (uint8_t)v;
            vis.cc[k].ms = e->ms | 1u;
            cc_sort();
            break;
        }
        vis.cc[k].val = (uint8_t)v;    /* a row stays where it is: only its value moves */
        vis.cc[k].ms = e->ms | 1u;
        break;
    case 0xD:
        vis.cat[c] = (uint8_t)n;
        vis.cat_ch = (uint8_t)c;
        vis.cat_ms = e->ms | 1u;
        break;
    case 0xE:
        vis.bend[c] = (int16_t)((int32_t)(n | v << 7) - 8192);
        vis.bend_ch = (uint8_t)c;
        vis.bend_ms = e->ms | 1u;
        break;
    }
}

/* the graphs move on (app_frame) */
static void views_tick(uint32_t now)
{
    if (now - vis.hist_t < HIST_MS)
        return;
    vis.hist_t = now;
    vis.hist_b[vis.hist_i] = (int8_t)(vis.bend[vis.bend_ch] / 128);
    vis.hist_a[vis.hist_i] = vis.cat[vis.cat_ch];
    vis.hist_i = (vis.hist_i + 1u) % HIST;
}

static uint16_t vel_col(uint32_t v) { return v >= 96u ? G_GLOW : v >= 48u ? G_HI : G_MID; }
static uint16_t age_col(uint32_t ms, uint32_t now)   /* 0: never */
{
    uint32_t dt = now - ms;
    return !ms ? 0 : dt < 150u ? G_GLOW : dt < 1000u ? G_HI : dt < 5000u ? G_MID : G_DIM;
}

/* ------------------------------------------------------------------ KEYS --- */
#define KG_Y 15                        /* the channels' rows (8 at most) */
#define KG_ROW 11
#define KG_X 16                        /* 128 notes in 224 px: 7/4 px a note */
#define KB_BARS 132                    /* the bars' foot, 24 px high at most */
#define KB_Y 134                       /* the zoomed keyboard: 3 octaves, 21 white keys */
#define KB_H 44
#define KB_X 4
#define KB_W 11
static const uint8_t WI[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6}, BK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static void draw_keys(void)
{
    char t[48], *d;
    uint32_t c, n, k, held = 0, mask = 0, lo = 128, hi = 0, vel[36], at[36];
    int32_t y = KG_Y, x, row;
    for (c = 0; c < 16u; c++)
        for (n = 0; n < 128u; n++)
            if (vk_vel[c][n]) {
                held++;
                lo = n < lo ? n : lo;
                hi = n > hi ? n : hi;
            }
    if (held && (lo < vis.zoom || hi >= vis.zoom + 36u)) {   /* the keyboard follows the notes */
        vis.zoom = lo / 12u * 12u;
        if (vis.zoom > 84u)
            vis.zoom = 84u;
    }
    d = s_cat(t, "HELD ");
    d = s_dec(d, (int32_t)held, 0);
    d = s_cat(d, "   DUP ON ");
    d = s_dec(d, (int32_t)vis.dup_on, 0);
    d = s_cat(d, "   OFF W/O ON ");
    s_dec(d, (int32_t)vis.orphan_off, 0);
    cv_text(4, 0, &FONT_XS, t, vis.dup_on || vis.orphan_off ? G_HI : G_MID);
    for (k = 0; k < 8u; k++) {         /* the 8 channels with the latest notes, in channel order */
        int best = -1;
        for (c = 0; c < 16u; c++)
            if (vis.ch_ms[c] && !(mask >> c & 1u) && (best < 0 || vis.ch_ms[c] - vis.ch_ms[best] < 0x80000000u))
                best = (int)c;
        if (best < 0)
            break;
        mask |= 1u << best;
    }
    for (c = 0, k = 0; c < 16u; c++)
        k += mask >> c & 1u;
    row = k > 4u ? KG_ROW : 2 * KG_ROW;   /* (four channels or fewer: rows twice as tall) */
    if (!mask)
        cv_text(4, KG_Y + 30, &FONT_XS, "NO NOTES YET", G_FAINT);
    for (c = 0; c < 16u; c++) {
        int any = 0;
        if (!(mask >> c & 1u))
            continue;
        for (n = 0; n < 128u; n += 12u)  /* the Cs */
            cv_rect(KG_X + (int32_t)(n * 7u / 4u), y + 1, 1, row - 2, G_FAINT);
        cv_rect(KG_X, y + row - 1, 224, 1, G_FAINT);
        for (n = 0; n < 128u; n++)
            if (vk_vel[c][n]) {
                cv_rect(KG_X + (int32_t)(n * 7u / 4u), y, 2, row - 1, vel_col(vk_vel[c][n]));
                any = 1;
            }
        s_dec(t, (int32_t)c + 1, 2);
        cv_text(0, y + (row - 12) / 2, &FONT_XS, t, any ? G_HI : G_DIM);
        y += row;
    }
    x = KG_X + (int32_t)(vis.zoom * 7u / 4u);   /* where the keyboard below is */
    cv_rect(x, KG_Y + 8 * KG_ROW + 1, 36 * 7 / 4, 2, G_DIM);
    for (k = 0; k < 36u; k++) {        /* the zoomed notes: the loudest of the channels, its pressure */
        vel[k] = at[k] = 0;
        for (c = 0; c < 16u; c++) {
            uint32_t v = vk_vel[c][vis.zoom + k], a = vk_at[c][vis.zoom + k];
            vel[k] = v > vel[k] ? v : vel[k];
            at[k] = a > at[k] ? a : at[k];
        }
    }
    for (k = 0; k < 36u; k++) {        /* the white keys, then the black over them */
        if (BK[k % 12u])
            continue;
        x = KB_X + (int32_t)((k / 12u) * 7u + WI[k % 12u]) * KB_W;
        cv_rect(x, KB_Y, KB_W - 1, KB_H, vel[k] ? vel_col(vel[k]) : G_DIM);
    }
    for (k = 0; k < 36u; k++) {
        if (!BK[k % 12u])
            continue;
        x = KB_X + (int32_t)((k / 12u) * 7u + WI[k % 12u] + 1u) * KB_W - 4;
        cv_rect(x - 1, KB_Y, 9, 28, G_BG);
        if (vel[k])
            cv_rect(x, KB_Y, 7, 27, vel_col(vel[k]));
        else                           /* (dark, an edge to tell it by) */
            cv_rect(x, KB_Y + 26, 7, 1, G_FAINT);
    }
    for (k = 0; k < 36u; k++) {        /* above each held key: its velocity, a bright tick at its pressure */
        int32_t bx, bw, h;
        if (!vel[k])
            continue;
        if (BK[k % 12u]) {
            bx = KB_X + (int32_t)((k / 12u) * 7u + WI[k % 12u] + 1u) * KB_W - 2;
            bw = 3;
        } else {
            bx = KB_X + (int32_t)((k / 12u) * 7u + WI[k % 12u]) * KB_W + 2;
            bw = 6;
        }
        h = (int32_t)vel[k] * 24 / 127 + 1;
        cv_rect(bx, KB_BARS - h, bw, h, vel_col(vel[k]));
        if (at[k]) {
            h = (int32_t)at[k] * 24 / 127 + 1;
            cv_rect(bx - 1, KB_BARS - h, bw + 2, 2, G_GLOW);
        }
    }
    for (k = 0; k < 3u; k++) {         /* the octaves' names under their Cs */
        s_note(t, vis.zoom + k * 12u, set.midc);
        cv_text(KB_X + (int32_t)k * 7 * KB_W, KB_Y + KB_H + 2, &FONT_XS, t, G_DIM);
    }
}

/* -------------------------------------------------------------------- CC --- */
#define GR_Y 14                        /* the graphs: bend (left), channel pressure (right) */
#define GR_H 60
static void draw_cc(void)
{
    char t[32], *d;
    uint32_t i, now = plat_ms();
    int32_t x, y, py = 0, mid = GR_Y + GR_H / 2;
    d = t;                             /* CH1 BEND +1024, CH1 AT 64: the channel that sent it last */
    if (vis.bend_ms) {
        d = s_cat(d, "CH");
        d = s_dec(d, vis.bend_ch + 1, 0);
        *d++ = ' ';
    }
    d = s_cat(d, "BEND ");
    if (vis.bend_ms) {
        if (vis.bend[vis.bend_ch] > 0)
            *d++ = '+';
        s_dec(d, vis.bend[vis.bend_ch], 0);
    }
    cv_text(2, 0, &FONT_XS, t, vis.bend_ms ? age_col(vis.bend_ms, now) : G_DIM);
    d = t;
    if (vis.cat_ms) {
        d = s_cat(d, "CH");
        d = s_dec(d, vis.cat_ch + 1, 0);
        *d++ = ' ';
    }
    d = s_cat(d, "AT ");
    if (vis.cat_ms)
        s_dec(d, vis.cat[vis.cat_ch], 0);
    cv_text(122, 0, &FONT_XS, t, vis.cat_ms ? age_col(vis.cat_ms, now) : G_DIM);
    cv_rect(2, mid, HIST, 1, G_FAINT);
    cv_rect(2, GR_Y, HIST, 1, G_FAINT);
    cv_rect(2, GR_Y + GR_H - 1, HIST, 1, G_FAINT);
    cv_rect(122, GR_Y + GR_H - 1, HIST, 1, G_FAINT);
    cv_rect(122, GR_Y, HIST, 1, G_FAINT);
    for (i = 0; i < HIST; i++) {       /* oldest on the left */
        uint32_t j = (vis.hist_i + i) % HIST;
        x = 2 + (int32_t)i;
        y = mid - vis.hist_b[j] * (GR_H / 2 - 2) / 64;
        if (i)
            cv_line(x - 1, py, x, y, G_HI);
        py = y;
        y = (int32_t)vis.hist_a[j] * (GR_H - 2) / 127;
        if (y)
            cv_rect(120 + x, GR_Y + GR_H - 1 - y, 1, y, G_MID);
    }
    y = GR_Y + GR_H + 6;
    if (!vis.ncc)
        cv_text(4, y + 20, &FONT_XS, "NO CONTROLLERS YET", G_FAINT);
    for (i = 0; i < vis.ncc; i++, y += 12) {   /* channel, number, name, a bar, the value */
        uint16_t c = age_col(vis.cc[i].ms, now);
        int32_t w = vis.cc[i].val * 100 / 127;
        s_dec(t, vis.cc[i].ch + 1, 2);
        cv_text(2, y, &FONT_XS, t, G_DIM);
        s_dec(t, vis.cc[i].cc, 3);
        cv_text(20, y, &FONT_XS, t, c == G_DIM ? G_DIM : G_MID);
        cv_text(44, y, &FONT_XS, cc_name(vis.cc[i].cc), c == G_DIM ? G_DIM : G_MID);
        cv_rect(102, y + 9, 100, 1, G_FAINT);
        if (w)
            cv_rect(102, y + 2, w, 7, c == G_GLOW ? G_HI : c == G_DIM ? G_DIM : G_MID);
        s_dec(t, vis.cc[i].val, 3);
        cv_text(210, y, &FONT_XS, t, c);
    }
}

/* ----------------------------------------------------------------- CHANS --- */
static void draw_chans(void)
{
    static const char *const SH[NTAPS] = {"USB", "TRS", "B-P", "B-C"};
    char t[32];
    uint32_t c, s, now = plat_ms();
    int32_t y = 14;
    cv_text(2, 0, &FONT_XS, "CH", G_DIM);
    for (s = 0; s < NTAPS; s++)
        cv_text(22 + (int32_t)s * 26, 0, &FONT_XS, SH[s], set.srcs >> s & 1u ? G_MID : G_FAINT);
    cv_text(128, 0, &FONT_XS, "LAST", G_DIM);
    for (c = 0; c < 16u; c++, y += 11) {
        uint32_t last = 0, i = vis.ch_ev[c];
        for (s = 0; s < NTAPS; s++) {  /* each source's cell: bright when it just sent, fading */
            uint16_t col = age_col(vis.src_ms[s][c], now);
            int32_t x = 22 + (int32_t)s * 26;
            if (col)
                cv_rect(x, y + 2, 18, 7, col);
            else
                cv_rect(x, y + 5, 18, 1, G_FAINT);
            if (vis.src_ms[s][c] && vis.src_ms[s][c] - last < 0x80000000u)
                last = vis.src_ms[s][c];
        }
        s_dec(t, (int32_t)c + 1, 2);
        cv_text(2, y - 1, &FONT_XS, t, last ? G_MID : G_FAINT);
        if (i && i <= ev_w && ev_w - (i - 1u) <= EV_RING) {
            ev_human(ev_at(i - 1u), t, set.midc, 18);
            cv_text(128, y - 1, &FONT_XS, t, age_col(last, now));
        }
    }
}

/* ----------------------------------------------------------------- CLOCK --- */
static void draw_clock(void)
{
    static const char *const RATE[4] = {"24", "25", "29.97D", "30"};
    char t[48], *d;
    uint32_t now = plat_ms(), i, n, lo = 0xFFFFu, hi = 0, beat;
    int on = ui.bpm10 && now - ui.clk_ms < 1000u;
    int32_t y;
    if (on) {
        d = s_dec(t, (int32_t)(ui.bpm10 / 10u), 0);
        *d++ = '.';
        s_dec(d, (int32_t)(ui.bpm10 % 10u), 0);
    } else {
        s_cat(t, "--.-");
    }
    cv_text(4, 2, &FONT_L, t, on ? G_HI : G_DIM);
    cv_text(110, 4, &FONT_S, "BPM", G_MID);
    cv_text(110, 20, &FONT_XS, on ? SRC_NAME[ui.clk_src] : "NO CLOCK", on ? G_MID : G_FAINT);
    if (on)
        text_inv(170, 20, &FONT_XS, ui.running ? "RUNNING" : "STOPPED", ui.running ? G_HI : G_DIM);
    y = 44;
    beat = vis.pos / 24u % 4u;         /* the beat in the bar: lit while running */
    for (i = 0; i < 4u; i++) {
        int lit = on && ui.running && i == beat;
        cv_rect(4 + (int32_t)i * 58, y, 54, 14, lit ? (i ? G_HI : G_GLOW) : G_FAINT);
    }
    y += 24;
    cv_text(4, y + 6, &FONT_XS, "POS", G_DIM);
    d = s_dec(t, (int32_t)(vis.pos / 96u + 1u), 0);    /* bar . beat . 16th */
    *d++ = '.';
    d = s_dec(d, (int32_t)(vis.pos / 24u % 4u + 1u), 0);
    *d++ = '.';
    s_dec(d, (int32_t)(vis.pos / 6u % 4u + 1u), 0);
    cv_text(30, y, &FONT_M, t, G_HI);
    if (vis.spp_ms) {
        d = s_cat(t, "SPP ");
        s_dec(d, (int32_t)vis.spp, 0);
        cv_text(160, y + 6, &FONT_XS, t, age_col(vis.spp_ms, now));
    }
    y += 32;
    n = ui.clk_i < 24u ? ui.clk_i : 24u;   /* the clocks' spacing, the last beat of them */
    for (i = 0; i < n; i++) {
        lo = ui.clk_dt[i] < lo ? ui.clk_dt[i] : lo;
        hi = ui.clk_dt[i] > hi ? ui.clk_dt[i] : hi;
    }
    d = s_cat(t, "TICK MS ");
    if (on && n) {
        d = s_cat(d, " MIN ");
        d = s_dec(d, (int32_t)lo, 0);
        d = s_cat(d, "  AVG ");
        d = s_dec(d, (int32_t)(ui.clk_sum * 10u / n / 10u), 0);
        *d++ = '.';
        d = s_dec(d, (int32_t)(ui.clk_sum * 10u / n % 10u), 0);
        d = s_cat(d, "  MAX ");
        s_dec(d, (int32_t)hi, 0);
    } else {
        s_cat(d, " --");
    }
    cv_text(4, y, &FONT_XS, t, on ? G_MID : G_DIM);
    y += 16;
    d = s_cat(t, "CLOCKS ");
    s_dec(d, (int32_t)ui.clk_n, 0);
    cv_text(4, y, &FONT_XS, t, G_DIM);
    y += 22;
    cv_text(4, y + 2, &FONT_XS, "MTC", G_DIM);
    if (vis.mtc_ms) {
        d = t;
        for (i = 0; i < 4u; i++) {
            if (i)
                *d++ = ':';
            *d++ = (char)('0' + vis.mtc[i] / 10u % 10u);
            *d++ = (char)('0' + vis.mtc[i] % 10u);
        }
        *d = 0;
        cv_text(30, y - 2, &FONT_S, t, age_col(vis.mtc_ms, now) == G_DIM ? G_DIM : G_HI);
        d = s_cat(t, RATE[vis.mtc_rate & 3u]);
        s_cat(d, " FPS");
        cv_text(130, y + 2, &FONT_XS, t, G_MID);
    } else {
        cv_text(30, y + 2, &FONT_XS, "--", G_DIM);
    }
    y += 22;
    if (vis.song_ms) {
        d = s_cat(t, "SONG ");
        s_dec(d, (int32_t)vis.song, 0);
        cv_text(4, y, &FONT_XS, t, age_col(vis.song_ms, now));
    }
}

static void views_draw(void)
{
    if (set.view == VIEW_KEYS)
        draw_keys();
    else if (set.view == VIEW_CC)
        draw_cc();
    else if (set.view == VIEW_CHANS)
        draw_chans();
    else
        draw_clock();
}
