/* SPDX-License-Identifier: GPL-3.0-only
 * BIGORNA (build 55, after TONITOR's backup.c): the sessions, their patterns and loops, and GLOBAL SETUP over USB
 * MIDI, for the web page that keeps them on a computer and puts them back (docs/backup.html, js/backup.js). The
 * installer's framing (ota.c: F0 pack7(00 59 cmd len24 body ~sum) F7), TONITOR's commands:
 *   0x50  list                       -> 0x50  count, then a record each: number (u16), length (u32)
 *   0x51  read  number off(u32) n(u16, <= 256)   -> 0x51  number off n, then n bytes of the record
 *   0x52  write number len(u32)      -> 0x52  status
 *   0x53  data  off(u32) bytes (<= 256, in order) -> 0x53  status
 *   0x54  commit                     -> 0x54  status: checked, written
 * A record is a sector as the flash holds it, its head first (projects.c): its number the head's own "what" --
 * session k's sector s (0 the session, 1 .. 3 its patterns) k x 16 + s; its loops' sector at place j in their
 * stream 0x1000 + k x 16 + j; GLOBAL SETUP 0x2000. Listed: every sector whose head checks, and the loop sectors a
 * session holds (none another's, none free).
 * Back, each is checked whole (its head, its length, its body's CRC, its number) before it is written. A session's
 * loops go first: written into free sectors with a new generation; then its patterns; then the session, pointed at
 * that generation (a session whose loops did not come: none) -- so a restore cut off leaves the session before it,
 * as a save does. Statuses: 0 done, 1 no flash, 2 not as asked, 3 no room, 4 not a whole record, 5 the flash failed.
 * All in the main loop; the record comes into prj_buf (no session is read or saved meanwhile). */

static int bk_wire(const uint8_t *p, uint32_t n);   /* one F0 .. F7 message to the host (main.c; host.c) */

#define BK_DATA 256u
#define BK_LOOP 0x1000u
#define BK_SET 0x2000u
#define BK_MAX (PRJ_SLOTS * PRJ_SECTS + LOP_N + 1u)
static uint8_t bk_msg[7u + 1u + BK_MAX * 6u], bk_out[2u + (sizeof bk_msg * 8u + 6u) / 7u];
static struct {
    uint8_t on;                  /* a record coming */
    uint16_t num;
    uint32_t len, got;
    uint32_t old[PRJ_SLOTS], gen[PRJ_SLOTS];   /* each slot's loops coming: the generation they had, and their new */
    uint32_t taken;              /* loop sectors written since the list (a bit each) */
} bk;

static uint32_t bk_rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void bk_wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static void bk_send(uint32_t cmd, uint32_t n)        /* the body already in bk_msg + 6 */
{
    uint32_t i, s = 0, o = 1, acc = 0, nb = 0;
    bk_msg[0] = 0;
    bk_msg[1] = 0x59;
    bk_msg[2] = (uint8_t)cmd;
    bk_msg[3] = (uint8_t)n;
    bk_msg[4] = (uint8_t)(n >> 8);
    bk_msg[5] = (uint8_t)(n >> 16);
    for (i = 0; i < n; i++)
        s += bk_msg[6u + i];
    bk_msg[6u + n] = (uint8_t)~s;
    bk_out[0] = 0xF0;
    for (i = 0; i < 7u + n; i++) {                   /* pack7, as ota.c's */
        acc |= (uint32_t)bk_msg[i] << nb;
        nb += 8u;
        while (nb >= 7u) {
            bk_out[o++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            nb -= 7u;
        }
    }
    if (nb)
        bk_out[o++] = (uint8_t)(acc & 0x7Fu);
    bk_out[o++] = 0xF7;
    bk_wire(bk_out, o);
}
static void bk_status(uint32_t cmd, uint32_t st)
{
    bk_msg[6] = (uint8_t)st;
    bk_send(cmd, 1u);
}
static int bk_head_ok(const prj_head_t *h)       /* (its own CRC, as any of ours) */
{
    return h->format == PRJ_FORMAT && h->len <= PRJ_SECT - sizeof(prj_head_t) &&
           h->head_crc == prj_crc((const uint8_t *)h, __builtin_offsetof(prj_head_t, head_crc));
}
/* record num: where it is in the flash and its length (0: none) */
static uint32_t bk_where(uint32_t num, uint32_t *len)
{
    prj_head_t h;
    uint32_t a = 0, magic = 0, i;
    *len = 0;
    if (num < PRJ_SLOTS * 16u && num % 16u < PRJ_SECTS) {
        a = prj_sector(num / 16u, num % 16u);
        magic = num % 16u ? PAT_MAGIC : PRJ_MAGIC;
    } else if (num == BK_SET) {
        a = SET_SECT;
        magic = SET_MAGIC;
    } else if (num >= BK_LOOP && num < BK_LOOP + PRJ_SLOTS * 16u) {
        for (i = 0; i < LOP_N; i++) {
            lop_meta_t m;
            if (lop_live(i) && !lop_peek(i, &m) && BK_LOOP + m.slot * 16u + m.idx == num)
                break;
        }
        if (i == LOP_N)
            return 0;
        a = lop_sector(i);
        magic = LOP_MAGIC;
    } else
        return 0;
    if (prj_flash_read(a, &h, sizeof h) || h.magic != magic || h.what != (magic == SET_MAGIC ? 0u : num) ||
        !bk_head_ok(&h))
        return 0;
    if (magic == PRJ_MAGIC && !prj_has[num / 16u])
        return 0;
    *len = magic == LOP_MAGIC ? PRJ_SECT : sizeof h + h.len;
    return a;
}
/* a loop sector for slot k to go to: a free one not taken by this restore, else one of k's own (the session it
 * replaces), else none */
static int bk_loop_place(uint32_t k)
{
    uint32_t i, pass;
    for (pass = 0; pass < 2u; pass++)
        for (i = 0; i < LOP_N; i++) {
            lop_meta_t m;
            int live = lop_live(i);
            if (bk.taken >> i & 1u)
                continue;
            if (pass == 0u ? !live : live && !lop_peek(i, &m) && m.slot == k)
                return (int)i;
        }
    return -1;
}
/* the record in prj_buf, whole: checked, then written where it goes */
static uint32_t bk_commit(void)
{
    prj_head_t *h = (prj_head_t *)(void *)prj_buf;
    uint32_t num = bk.num, k = num / 16u % PRJ_SLOTS;
    if (!bk_head_ok(h) || sizeof *h + h->len > bk.len || h->body_crc != prj_crc(prj_buf + sizeof *h, h->len))
        return 4u;
    if (num == BK_SET) {                             /* GLOBAL SETUP: as it was, read again */
        if (h->magic != SET_MAGIC || h->what)
            return 4u;
        if (prj_flash_erase(SET_SECT) || prj_flash_prog(SET_SECT, prj_buf, sizeof *h + h->len))
            return 5u;
        setup_load();
        return 0;
    }
    if (num >= BK_LOOP) {                            /* a session's loops: a new generation, a free sector */
        lop_meta_t *m = lop_meta();
        int i;
        if (h->magic != LOP_MAGIC || h->what != num || h->len != LOP_BODY || m->slot != k ||
            m->idx != (num & 15u) || !m->nsec || m->nsec > 8u)
            return 4u;
        if (bk.old[k] != m->gen || !bk.gen[k]) {
            bk.old[k] = m->gen;
            bk.gen[k] = ++lop_gmax;
        }
        m->gen = bk.gen[k];
        if ((i = bk_loop_place(k)) < 0)
            return 3u;
        if (lop_write((uint32_t)i))
            return 5u;
        bk.taken |= 1u << i;
        return 0;
    }
    if (num % 16u) {                                 /* its patterns, as they were */
        if (h->magic != PAT_MAGIC || h->what != num || h->len != PAT_PER * sizeof(pat_t))
            return 4u;
    } else {                                         /* the session, at its loops' new generation (or none) */
        uint32_t go = 0, no = 0, g;
        if (h->magic != PRJ_MAGIC || h->what != num)
            return 4u;
        if (h->len == sizeof(prj_body_t) || h->len == PRJ_LEN50) {
            go = __builtin_offsetof(prj_body_t, loop_gen);
            no = __builtin_offsetof(prj_body_t, loop_nsec);
        } else if (h->len == PRJ_LEN49) {            /* (builds 38 .. 49) */
            go = __builtin_offsetof(prj_body49_t, loop_gen);
            no = __builtin_offsetof(prj_body49_t, loop_nsec);
        }
        if (go) {
            uint8_t *b = prj_buf + sizeof *h;
            memcpy(&g, b + go, 4u);
            g = g && g == bk.old[k] ? bk.gen[k] : 0u;
            memcpy(b + go, &g, 4u);
            if (!g)
                b[no] = 0;
            prj_head_fill(PRJ_MAGIC, num, h->len);
        }
    }
    if (prj_flash_erase(prj_sector(k, num % 16u)) || prj_flash_prog(prj_sector(k, num % 16u), prj_buf, sizeof *h + h->len))
        return 5u;
    if (num % 16u == 0u) {
        prj_scan(prj_ok);                            /* (it is there; its loops are its) */
        bk.old[k] = bk.gen[k] = 0;
    }
    return 0;
}
static void bk_done(uint32_t num)                    /* (the screen says what came back) */
{
    char m[16] = "RESTORED ";
    if (num == BK_SET)
        str_cpy(m + 9, "SETUP", 7);
    else if (num < BK_LOOP && num % 16u == 0u) {
        str_cpy(m, "RESTORED S", 12);
        fmt_int(m + 10, (int32_t)(num / 16u + 1u));
    } else
        return;
    say(m, 1500);
    ui.force = 1;
}
/* a message from the host (decoded: 00 59 cmd len24 body sum) */
static void bk_handle(const uint8_t *m, uint32_t d)
{
    uint32_t cmd = m[2], n = d - 7u, i;
    const uint8_t *b = m + 6;
    if (cmd == 0x50u) {                              /* the list (and a restore's start: nothing taken yet) */
        uint32_t k = 0, num, len;
        bk.on = 0;
        bk.taken = 0;
        memset(bk.old, 0, sizeof bk.old);
        memset(bk.gen, 0, sizeof bk.gen);
        for (num = 0; num < PRJ_SLOTS * 16u; num++)
            if (num % 16u < PRJ_SECTS && bk_where(num, &len) && k < BK_MAX) {
                bk_msg[7u + k * 6u] = (uint8_t)num;
                bk_msg[8u + k * 6u] = (uint8_t)(num >> 8);
                bk_wr32(bk_msg + 9u + k * 6u, len);
                k++;
            }
        for (num = BK_LOOP; num < BK_LOOP + PRJ_SLOTS * 16u; num++)
            if (bk_where(num, &len) && k < BK_MAX) {
                bk_msg[7u + k * 6u] = (uint8_t)num;
                bk_msg[8u + k * 6u] = (uint8_t)(num >> 8);
                bk_wr32(bk_msg + 9u + k * 6u, len);
                k++;
            }
        if (bk_where(BK_SET, &len) && k < BK_MAX) {
            bk_msg[7u + k * 6u] = (uint8_t)BK_SET;
            bk_msg[8u + k * 6u] = (uint8_t)(BK_SET >> 8);
            bk_wr32(bk_msg + 9u + k * 6u, len);
            k++;
        }
        bk_msg[6] = (uint8_t)k;
        bk_send(0x50u, 1u + k * 6u);
    } else if (cmd == 0x51u && n == 8u) {            /* a piece of a record */
        uint32_t num = b[0] | (uint32_t)b[1] << 8, off = bk_rd32(b + 2), cnt = b[6] | (uint32_t)b[7] << 8, len;
        uint32_t a = bk_where(num, &len);
        cnt = !a || off >= len ? 0u : cnt > BK_DATA ? BK_DATA : cnt;
        cnt = off + cnt > len ? len - off : cnt;
        memcpy(bk_msg + 6, b, 6);
        bk_msg[12] = (uint8_t)cnt;
        bk_msg[13] = (uint8_t)(cnt >> 8);
        if (cnt)
            prj_flash_read(a + off, bk_msg + 14, cnt);
        bk_send(0x51u, 8u + cnt);
    } else if (cmd == 0x52u && n == 6u) {            /* a record to come */
        uint32_t num = b[0] | (uint32_t)b[1] << 8, len = bk_rd32(b + 2);
        int ok = (num < PRJ_SLOTS * 16u && num % 16u < PRJ_SECTS) || num == BK_SET ||
                 (num >= BK_LOOP && num < BK_LOOP + PRJ_SLOTS * 16u && (num & 15u) < 8u);
        bk.on = 0;
        if (!prj_ok) {
            bk_status(0x52u, 1u);
            return;
        }
        if (!ok || len < sizeof(prj_head_t) || len > PRJ_SECT) {
            bk_status(0x52u, 2u);
            return;
        }
        memset(prj_buf, 0, PRJ_SECT);
        bk.on = 1;
        bk.num = (uint16_t)num;
        bk.len = len;
        bk.got = 0;
        bk_status(0x52u, 0u);
    } else if (cmd == 0x53u && n >= 4u) {            /* its bytes, in order */
        uint32_t off = bk_rd32(b), cnt = n - 4u;
        if (!bk.on || off != bk.got || off + cnt > bk.len || cnt > BK_DATA) {
            bk.on = 0;
            bk_status(0x53u, 2u);
            return;
        }
        for (i = 0; i < cnt; i++)
            prj_buf[bk.got++] = b[4u + i];
        bk_status(0x53u, 0u);
    } else if (cmd == 0x54u) {                       /* whole: checked, written */
        uint32_t st = !bk.on || bk.got != bk.len ? 2u : bk_commit();
        bk.on = 0;
        bk_status(0x54u, st);
        if (!st)
            bk_done(bk.num);
    }
}
