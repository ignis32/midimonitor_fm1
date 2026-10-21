/* SPDX-License-Identifier: MPL-2.0 */
/* The MIDI tap: every MIDI byte that comes in, from each source, as it came, stamped with the millisecond it arrived
 * (plat.h plat_tap). plat_midi_in leaves out what an instrument does not need (SysEx, system common, active sensing,
 * reset, stray bytes); the tap keeps all of it, for a monitor. Part of the unity build on the device (before usb.c,
 * midi_uart.c and radio.c, which feed it) and on the host (host/plat_host.c's "midi" and "raw").
 *
 * Producers: USB and the TRS jack from TIMER5, Bluetooth from the main loop under plat_lock. One consumer: the main
 * loop (plat_tap). A full ring drops the new entry and counts it against its source. */
#define FM1B_MIDI_TAP 1
#define TAP_RING 1024u                 /* entries (a power of two) */
static uint32_t tap_q[TAP_RING][2] PLAT_POOL;
static volatile uint32_t tap_w, tap_r;
static uint32_t tap_drop[NTAPS], tap_bytes[NTAPS];

#define TAP_W(kind, src, n, b0, b1, b2) \
    ((uint32_t)(b0) | (uint32_t)(b1) << 8 | (uint32_t)(b2) << 16 | (uint32_t)(n) << 24 | (uint32_t)(src) << 26 | \
     (uint32_t)(kind) << 28)

static void tap_put(uint32_t ms, uint32_t w)
{
    uint32_t at;
    if (tap_w - tap_r >= TAP_RING) {
        tap_drop[TAP_SRC(w)]++;
        return;
    }
    at = tap_w % TAP_RING;
    tap_q[at][0] = ms;
    tap_q[at][1] = w;
    __asm__ volatile("" ::: "memory");
    tap_w++;
}

int plat_tap(uint32_t *ms, uint32_t *w)
{
    uint32_t at;
    if (tap_r == tap_w)
        return 0;
    at = tap_r % TAP_RING;
    *ms = tap_q[at][0];
    *w = tap_q[at][1];
    __asm__ volatile("" ::: "memory");
    tap_r++;
    return 1;
}
uint32_t plat_tap_drops(int src) { return src >= 0 && src < NTAPS ? tap_drop[src] : 0u; }
uint32_t plat_tap_bytes(int src) { return src >= 0 && src < NTAPS ? tap_bytes[src] : 0u; }

/* ---- a MIDI byte stream (the TRS jack; Bluetooth once its timestamps are taken out) into entries */
typedef struct {
    uint8_t cur;                       /* the status the data bytes belong to (0: none) */
    uint8_t need, got, fresh;          /* data bytes it takes, got so far; fresh: its status byte was on the wire */
    uint8_t d[2];
    uint8_t sx, sxn, sxb[3];           /* inside SysEx; the bytes not yet put */
} tap_parser_t;

static uint32_t tap_need(uint32_t st)  /* data bytes after a status byte */
{
    if (st < 0xF0u)
        return (st & 0xE0u) == 0xC0u ? 1u : 2u;
    return st == 0xF2u ? 2u : st == 0xF1u || st == 0xF3u ? 1u : 0u;
}

static void tap_sx_flush(tap_parser_t *p, int src, uint32_t ms)
{
    if (p->sxn)
        tap_put(ms, TAP_W(TK_SX, src, p->sxn, p->sxb[0], p->sxb[1], p->sxb[2]));
    p->sxn = 0;
}
static void tap_sx_add(tap_parser_t *p, int src, uint32_t b, uint32_t ms)
{
    p->sxb[p->sxn++] = (uint8_t)b;
    if (p->sxn == 3u)
        tap_sx_flush(p, src, ms);
}

static void tap_byte(tap_parser_t *p, int src, uint32_t b, uint32_t ms)
{
    tap_bytes[src]++;
    if (b >= 0xF8u) {                  /* real time: anywhere, even inside a message or SysEx */
        if (p->sx)
            tap_sx_flush(p, src, ms);
        tap_put(ms, TAP_W(TK_MSG, src, 1, b, 0, 0));
        return;
    }
    if (b == 0xF7u && p->sx) {         /* SysEx ends */
        tap_sx_add(p, src, b, ms);
        tap_sx_flush(p, src, ms);
        p->sx = 0;
        return;
    }
    if (b & 0x80u) {                   /* a status byte */
        if (p->sx) {                   /* SysEx cut off without its F7 */
            tap_sx_flush(p, src, ms);
            tap_put(ms, TAP_W(TK_CUT, src, 1, 0xF0u, 0, 0));
            p->sx = 0;
        } else if (p->got) {           /* a message cut short: what came of it */
            tap_put(ms, TAP_W(TK_CUT, src, 1u + p->got, p->cur, p->d[0], 0));
        }
        p->got = 0;
        if (b == 0xF0u) {
            p->sx = 1;
            p->cur = 0;                /* SysEx ends running status */
            tap_sx_add(p, src, b, ms);
            return;
        }
        if (b == 0xF7u) {              /* an EOX with no SysEx */
            tap_put(ms, TAP_W(TK_STRAY, src, 1, b, 0, 0));
            p->cur = 0;
            return;
        }
        p->need = (uint8_t)tap_need(b);
        if (!p->need) {                /* F4 F5 F6: whole in one byte */
            tap_put(ms, TAP_W(TK_MSG, src, 1, b, 0, 0));
            p->cur = 0;
            return;
        }
        p->cur = (uint8_t)b;
        p->fresh = 1;
        return;
    }
    if (p->sx) {
        tap_sx_add(p, src, b, ms);
        return;
    }
    if (!p->cur) {                     /* a data byte with no status to belong to */
        tap_put(ms, TAP_W(TK_STRAY, src, 1, b, 0, 0));
        return;
    }
    p->d[p->got++] = (uint8_t)b;
    if (p->got < p->need)
        return;
    tap_put(ms, TAP_W(p->fresh ? TK_MSG : TK_RS, src, 1u + p->need, p->cur, p->d[0], p->need == 2u ? p->d[1] : 0u));
    p->got = 0;
    p->fresh = 0;
    if (p->cur >= 0xF0u)               /* system common keeps no running status */
        p->cur = 0;
}

static tap_parser_t tap_trs, tap_ble[2];

/* ---- USB-MIDI event packets (byte 0 = cable / CIN), as they come */
static void tap_usb(uint32_t pkt, uint32_t ms)
{
    uint32_t cin = pkt & 15u, b0 = pkt >> 8 & 0xFFu, b1 = pkt >> 16 & 0xFFu, b2 = pkt >> 24;
    static const uint8_t N[16] = {0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};
    uint32_t n = N[cin], kind = TK_MSG;
    if (!n)
        return;                        /* CIN 0, 1: reserved */
    tap_bytes[TAP_USB] += n;
    if (cin == 4u || cin == 6u || cin == 7u || (cin == 5u && b0 == 0xF7u))
        kind = TK_SX;
    else if (cin == 5u && b0 < 0xF1u)
        kind = TK_STRAY;
    else if (cin == 0xFu && b0 < 0x80u)
        kind = TK_STRAY;               /* a single data byte (CIN F carries any byte) */
    tap_put(ms, TAP_W(kind, TAP_USB, n, b0, n > 1u ? b1 : 0u, n > 2u ? b2 : 0u));
}

/* ---- a Bluetooth MIDI packet (a header, then timestamp + message, running status, SysEx across packets): the
 * timestamps out, the MIDI bytes into the link's parser. dev: from the device the FM-1 connected to (else from the
 * computer connected to the FM-1) */
static void tap_ble_packet(const uint8_t *b, uint32_t len, int dev, uint32_t ms)
{
    tap_parser_t *p = &tap_ble[!!dev];
    int src = dev ? TAP_BLE_DEV : TAP_BLE_HOST;
    uint32_t i = 1;
    if (len < 2u || !(b[0] & 0x80u))
        return;
    while (i < len) {
        if (b[i] & 0x80u) {            /* a timestamp's low byte; a byte with bit 7 after it is a status (or F7) */
            i++;
            if (i < len && (b[i] & 0x80u))
                tap_byte(p, src, b[i++], ms);
            continue;
        }
        tap_byte(p, src, b[i++], ms);  /* data: the message's, running status's or SysEx's */
    }
}
