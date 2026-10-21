/* SPDX-License-Identifier: MPL-2.0 */
/* The log: every message in, from the MIDI tap (plat_tap), as events in a ring, SysEx assembled per source; and
 * each event as text, in hex and in words. Part of app.c. */

enum { EV_MSG, EV_RS, EV_SYSEX, EV_SXCUT, EV_CUT, EV_STRAY, EV_LOST };
/* what an event is, for the filters: the black keys on the FILTER page, in this order; TY_ERR always shows */
enum { TY_NOTE, TY_PAT, TY_CC, TY_PC, TY_CAT, TY_PB, TY_SYSEX, TY_CLOCK, TY_TRANSPORT, TY_ACTIVE, TY_SYS, NTY, TY_ERR = NTY };
static const char *const TY_NAME[NTY] = {"NOTE", "POLY AT", "CC", "PROGRAM", "CH AT", "BEND", "SYSEX", "CLOCK",
                                         "TRANSPORT", "ACT SENSE", "SYSTEM"};
static const char *const SRC_NAME[NTAPS] = {"USB", "TRS", "BLE", "DEV"};

typedef struct {
    uint32_t ms;                       /* when it came (plat_ms) */
    uint32_t sxo;                      /* SysEx: its kept bytes' place in sx_ring (a running count); LOST: how many */
    uint16_t sxn, sxk;                 /* SysEx: its length, the bytes of it kept */
    uint8_t src, kind, type, n;        /* TAP_*, EV_*, TY_*, bytes in b */
    uint8_t b[3];
    uint8_t rsv;
} ev_t;

#define EV_RING 4096u                  /* events kept (a power of two) */
#define SX_RING 32768u                 /* SysEx bytes kept */
#define SX_KEEP 512u                   /* of each SysEx message at most */
static ev_t ev_ring[EV_RING] PLAT_POOL;
static uint8_t sx_ring[SX_RING] PLAT_POOL;
static uint32_t ev_w, sx_w;            /* running counts: events and SysEx bytes written */

static struct {                        /* a SysEx coming in, per source */
    uint8_t open;
    uint32_t ms, n;
    uint8_t b[SX_KEEP];
} sx_in[NTAPS];

static uint32_t src_events[NTAPS], src_lost_seen[NTAPS];

static uint32_t ev_type(uint32_t st)
{
    static const uint8_t CH[8] = {TY_NOTE, TY_NOTE, TY_PAT, TY_CC, TY_PC, TY_CAT, TY_PB, 0};
    if (st < 0xF0u)
        return CH[(st >> 4) & 7u];
    if (st == 0xF0u)
        return TY_SYSEX;
    if (st == 0xF8u)
        return TY_CLOCK;
    if (st == 0xFAu || st == 0xFBu || st == 0xFCu)
        return TY_TRANSPORT;
    if (st == 0xFEu)
        return TY_ACTIVE;
    return TY_SYS;
}

static void mon_event(const ev_t *e);  /* app.c: lights, clock, held notes */
static void thru(uint32_t w);          /* app.c: each tap entry out again (THRU) */

static ev_t *ev_new(uint32_t ms, uint32_t src, uint32_t kind, uint32_t type)
{
    ev_t *e = &ev_ring[ev_w % EV_RING];
    *e = (ev_t){0};
    e->ms = ms;
    e->src = (uint8_t)src;
    e->kind = (uint8_t)kind;
    e->type = (uint8_t)type;
    return e;
}
static void ev_done(ev_t *e)
{
    ev_w++;
    src_events[e->src]++;
    mon_event(e);
}

static void sx_end(uint32_t src, int whole)
{
    ev_t *e;
    uint32_t i, k;
    if (!sx_in[src].open)
        return;
    k = sx_in[src].n < SX_KEEP ? sx_in[src].n : SX_KEEP;
    e = ev_new(sx_in[src].ms, src, whole ? EV_SYSEX : EV_SXCUT, TY_SYSEX);
    e->sxo = sx_w;
    e->sxn = (uint16_t)(sx_in[src].n < 0xFFFFu ? sx_in[src].n : 0xFFFFu);
    e->sxk = (uint16_t)k;
    for (i = 0; i < k; i++)
        sx_ring[(sx_w + i) % SX_RING] = sx_in[src].b[i];
    sx_w += k;
    for (i = 0; i < 3u && i + 1u < k; i++)  /* the bytes after F0: the maker's ID */
        e->b[i] = sx_in[src].b[i + 1];
    sx_in[src].open = 0;
    ev_done(e);
}

static void sx_byte(uint32_t src, uint32_t b, uint32_t ms)
{
    if (b == 0xF0u || !sx_in[src].open) {
        sx_end(src, 0);                /* an F0 before the last one's F7 */
        sx_in[src].open = 1;
        sx_in[src].ms = ms;
        sx_in[src].n = 0;
    }
    if (sx_in[src].n < SX_KEEP)
        sx_in[src].b[sx_in[src].n] = (uint8_t)b;
    sx_in[src].n++;
    if (b == 0xF7u)
        sx_end(src, 1);
}

/* the tap's entries into the log (main loop) */
static void mon_poll(void)
{
    uint32_t ms, w, s, i;
    int budget = 512;                  /* a flood waits in the tap, the screen keeps going */
    for (s = 0; s < NTAPS; s++) {      /* the tap's ring was full: say how much was lost */
        uint32_t d = plat_tap_drops((int)s);
        if (d != src_lost_seen[s]) {
            ev_t *e = ev_new(plat_ms(), s, EV_LOST, TY_ERR);
            e->sxo = d - src_lost_seen[s];
            src_lost_seen[s] = d;
            ev_done(e);
        }
    }
    while (budget-- > 0 && plat_tap(&ms, &w)) {
        uint32_t src = TAP_SRC(w), kind = TAP_KIND(w), n = TAP_LEN(w), b0 = TAP_B(w, 0);
        ev_t *e;
        thru(w);
        if (kind == TK_SX) {
            for (i = 0; i < n; i++)
                sx_byte(src, TAP_B(w, i), ms);
            continue;
        }
        if (kind == TK_CUT && b0 == 0xF0u) {
            sx_end(src, 0);
            continue;
        }
        if (sx_in[src].open && !(kind == TK_MSG && b0 >= 0xF8u))
            sx_end(src, 0);            /* (over USB a message just ends it) */
        e = ev_new(ms, src, kind == TK_MSG ? EV_MSG : kind == TK_RS ? EV_RS : kind == TK_CUT ? EV_CUT : EV_STRAY,
                   kind == TK_MSG || kind == TK_RS ? ev_type(b0) : TY_ERR);
        e->n = (uint8_t)n;
        for (i = 0; i < n; i++)
            e->b[i] = (uint8_t)TAP_B(w, i);
        ev_done(e);
    }
}

static void mon_clear(void)
{
    uint32_t s;
    ev_w = 0;
    for (s = 0; s < NTAPS; s++)
        sx_in[s].open = 0;
}

static const ev_t *ev_at(uint32_t i) { return &ev_ring[i % EV_RING]; }   /* i < ev_w, ev_w - i <= EV_RING */
static uint32_t ev_oldest(void) { return ev_w > EV_RING ? ev_w - EV_RING : 0u; }
static int sx_kept(const ev_t *e) { return sx_w - e->sxo <= SX_RING; }   /* its bytes not yet written over */
static uint32_t sx_at(const ev_t *e, uint32_t i) { return sx_ring[(e->sxo + i) % SX_RING]; }

/* ------------------------------------------------------------------ text --- */
static const char HEXD[] = "0123456789ABCDEF";

static char *s_cat(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}
static char *s_hex2(char *d, uint32_t v)
{
    *d++ = HEXD[v >> 4 & 15u];
    *d++ = HEXD[v & 15u];
    *d = 0;
    return d;
}
static char *s_dec(char *d, int32_t v, int width)   /* right-aligned in width (0: as long as it is) */
{
    char t[12];
    int n = 0, neg = v < 0;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (neg)
        t[n++] = '-';
    while (width-- > n)
        *d++ = ' ';
    while (n)
        *d++ = t[--n];
    *d = 0;
    return d;
}
static char *s_pad(char *d, char *start, int width)  /* spaces up to width from start */
{
    while (d - start < width)
        *d++ = ' ';
    *d = 0;
    return d;
}

static char *s_note(char *d, uint32_t note, int midc)   /* midc: 3 or 4, the octave of note 60 */
{
    static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    d = s_cat(d, N[note % 12u]);
    return s_dec(d, (int32_t)(note / 12u) - (midc == 3 ? 2 : 1), 0);
}

static const char *cc_name(uint32_t cc)
{
    switch (cc) {
    case 0: return "BANK";
    case 1: return "MOD";
    case 2: return "BREATH";
    case 4: return "FOOT";
    case 5: return "PORTA T";
    case 6: return "DATA";
    case 7: return "VOLUME";
    case 8: return "BALANCE";
    case 10: return "PAN";
    case 11: return "EXPR";
    case 32: return "BANK L";
    case 38: return "DATA L";
    case 64: return "SUSTAIN";
    case 65: return "PORTA";
    case 66: return "SOSTEN";
    case 67: return "SOFT";
    case 71: return "RESO";
    case 74: return "CUTOFF";
    case 96: return "DATA+";
    case 97: return "DATA-";
    case 98: return "NRPN L";
    case 99: return "NRPN M";
    case 100: return "RPN L";
    case 101: return "RPN M";
    case 120: return "SND OFF";
    case 121: return "RESET";
    case 122: return "LOCAL";
    case 123: return "NOTES OFF";
    case 124: return "OMNI OFF";
    case 125: return "OMNI ON";
    case 126: return "MONO";
    case 127: return "POLY";
    }
    return "";
}

static const char *maker_name(const ev_t *e)
{
    if (e->sxk < 2u)
        return "";
    switch (e->b[0]) {
    case 0x7E: return "UNIV";
    case 0x7F: return "UNIV RT";
    case 0x7D: return "NON-COM";
    case 0x40: return "KAWAI";
    case 0x41: return "ROLAND";
    case 0x42: return "KORG";
    case 0x43: return "YAMAHA";
    case 0x44: return "CASIO";
    case 0x47: return "AKAI";
    case 0x18: return "E-MU";
    case 0x01: return "SEQUENT";
    case 0x04: return "MOOG";
    case 0x06: return "LEXICON";
    case 0x07: return "KURZWEIL";
    case 0x0F: return "ENSONIQ";
    case 0x10: return "OBERHEIM";
    case 0x22: return "M-VAVE";
    }
    if (e->b[0] == 0x00u && e->sxk >= 4u) {
        if (e->b[1] == 0x20u && e->b[2] == 0x29u) return "NOVATION";
        if (e->b[1] == 0x20u && e->b[2] == 0x6Bu) return "ARTURIA";
        if (e->b[1] == 0x20u && e->b[2] == 0x3Cu) return "ELEKTRON";
        if (e->b[1] == 0x01u && e->b[2] == 0x72u) return "BEHRINGER";
        if (e->b[1] == 0x00u && e->b[2] == 0x0Eu) return "ALESIS";
        if (e->b[1] == 0x21u && e->b[2] == 0x1Du) return "ABLETON";
    }
    return "";
}

/* the bytes as they came, as many as fit in max characters ("..": more) */
static void ev_hex(const ev_t *e, char *d, int max)
{
    char *s = d;
    uint32_t i, n;
    *d = 0;
    if (e->kind == EV_LOST) {
        s_cat(d, "-- --");
        return;
    }
    if (e->kind != EV_SYSEX && e->kind != EV_SXCUT) {
        for (i = 0; i < e->n; i++) {
            d = s_hex2(d, e->b[i]);
            if (i + 1u < e->n)
                *d++ = ' ';
        }
        *d = 0;
        return;
    }
    n = sx_kept(e) ? e->sxk : 0u;
    for (i = 0; i < n; i++) {
        if (d - s + 2 > max || (i + 1u < e->sxn && d - s + 5 > max)) {
            s_cat(d, "..");
            return;
        }
        d = s_hex2(d, sx_at(e, i));
        if (i + 1u < n)
            *d++ = ' ';
    }
    *d = 0;
    if (!n)
        s_cat(d, "F0 ..");
}

/* the event in words; ch: its channel 1..16 (0: none) */
static int ev_human(const ev_t *e, char *d, int midc)
{
    uint32_t st = e->b[0], d1 = e->b[1], d2 = e->b[2];
    char *s = d;
    *d = 0;
    switch (e->kind) {
    case EV_SYSEX:
    case EV_SXCUT:
        d = s_cat(d, e->kind == EV_SYSEX ? "SYSEX " : "SYSEX CUT ");
        if (*maker_name(e)) {
            d = s_cat(d, maker_name(e));
            *d++ = ' ';
        } else if (e->sxk >= 2u) {
            d = s_hex2(d, e->b[0]);
            *d++ = ' ';
        }
        d = s_dec(d, e->sxn, 0);
        s_cat(d, "B");
        return 0;
    case EV_CUT:
        s_cat(d, "CUT SHORT");
        return 0;
    case EV_STRAY:
        s_cat(d, st == 0xF7u ? "STRAY EOX" : "STRAY BYTE");
        return 0;
    case EV_LOST:
        d = s_cat(d, "LOST ");
        d = s_dec(d, (int32_t)e->sxo, 0);
        s_cat(d, " (FULL)");
        return 0;
    }
    if (st < 0xF0u) {
        int ch = (int)(st & 15u) + 1;
        switch (st >> 4) {
        case 0x8:
            d = s_cat(d, "NOTE OFF ");
            d = s_note(d, d1, midc);
            d = s_pad(d, s, 13);
            s_dec(d, (int32_t)d2, 3);
            break;
        case 0x9:
            d = s_cat(d, d2 ? "NOTE ON  " : "NOTE OFF ");
            d = s_note(d, d1, midc);
            d = s_pad(d, s, 13);
            d = s_dec(d, (int32_t)d2, 3);
            if (!d2)
                s_cat(d, "*");         /* a note-on at velocity 0 */
            break;
        case 0xA:
            d = s_cat(d, "POLY AT ");
            d = s_note(d, d1, midc);
            d = s_pad(d, s, 13);
            s_dec(d, (int32_t)d2, 3);
            break;
        case 0xB:
            d = s_cat(d, "CC");
            d = s_dec(d, (int32_t)d1, 4);
            *d++ = ' ';
            d = s_cat(d, cc_name(d1));
            d = s_pad(d, s, 17);
            s_dec(d, (int32_t)d2, 3);
            break;
        case 0xC:
            d = s_cat(d, "PROGRAM ");
            s_dec(d, (int32_t)d1, 0);
            break;
        case 0xD:
            d = s_cat(d, "CH AT ");
            s_dec(d, (int32_t)d1, 0);
            break;
        case 0xE: {
            int32_t v = (int32_t)(d1 | d2 << 7) - 8192;
            d = s_cat(d, "BEND ");
            if (v > 0)
                *d++ = '+';
            s_dec(d, v, 0);
            break;
        }
        }
        return ch;
    }
    switch (st) {
    case 0xF1:
        d = s_cat(d, "MTC QF ");
        d = s_dec(d, (int32_t)(d1 >> 4), 0);
        *d++ = ':';
        s_dec(d, (int32_t)(d1 & 15u), 0);
        break;
    case 0xF2:
        d = s_cat(d, "SONG POS ");
        s_dec(d, (int32_t)(d1 | d2 << 7), 0);
        break;
    case 0xF3:
        d = s_cat(d, "SONG SEL ");
        s_dec(d, (int32_t)d1, 0);
        break;
    case 0xF6: s_cat(d, "TUNE REQ"); break;
    case 0xF8: s_cat(d, "CLOCK"); break;
    case 0xFA: s_cat(d, "START"); break;
    case 0xFB: s_cat(d, "CONTINUE"); break;
    case 0xFC: s_cat(d, "STOP"); break;
    case 0xFE: s_cat(d, "ACT SENSE"); break;
    case 0xFF: s_cat(d, "RESET"); break;
    default:
        d = s_cat(d, "UNDEFINED ");
        s_hex2(d, st);
        break;
    }
    return 0;
}
