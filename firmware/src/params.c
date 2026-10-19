/* SPDX-License-Identifier: GPL-3.0-only
 * The parameter pages (EDIT): each page eight parameters, as the Digitone II's (KNOB 1 .. 4: A .. D; with SHIFT
 * held: E .. H): SYN 1 .. 4 of the track's machine, FLTR 1 and 2, AMP, FX. A parameter: its name, its range, its
 * centre (a bipolar one shows the distance from it), how its value reads. */

enum { PK_NUM, PK_BIP, PK_ALGO, PK_RATC, PK_RATA, PK_RATB, PK_ONOFF, PK_LIST, PK_HOLD, PK_PHC, PK_NONE };
typedef struct {
    const char *name;
    int16_t lo, hi, mid;
    uint8_t kind;
    const char *const *list;
} par_t;

static const char *const L_PHRT[] = {"OFF", "ALL", "C", "A+B", "A+B2"};
static const char *const L_TBL[] = {"PRIM", "HARM"};
static const char *const L_WMOD[] = {"OFF", "RING", "RING FIX", "SYNC"};
static const char *const L_RSET[] = {"OFF", "ON", "RAND"};
static const char *const L_NTYP[] = {"GRAIN", "TUNED", "S+H"};
static const char *const L_MOCT[] = {"-2", "-1", "0"};
static const char *const L_RT[] = {"PRE", "POST"};
static const char *const L_LEGT[] = {"LP", "HP"};
static const char *const L_MODE[] = {"AHD", "ADSR"};
static const char *const L_FLT[] = {"MULTI", "LP4", "LEGACY", "COMB-", "COMB+", "EQ"};
static const char *const L_DMODE[] = {"WRAP", "XOR", "FLIP", "DROP"};
static const char *const L_SLICE[] = {"OFF", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
                                      "15", "16"};
static const char *const L_SRATE[] = {"1/8", "1/16", "1/32", "1/16T"};

#define P_(n, lo, hi, mid, k) {n, lo, hi, mid, k, 0}
#define PL(n, hi, l) {n, 0, hi, 0, PK_LIST, l}
#define PN(n) {n, 0, 127, 0, PK_NUM, 0}
#define PB(n) {n, 0, 127, 64, PK_BIP, 0}
#define PO(n) {n, 0, 1, 0, PK_ONOFF, 0}
#define PX {"", 0, 0, 0, PK_NONE, 0}

static const par_t PAR_FT[4][8] = {
    {P_("ALGO", 0, 7, 0, PK_ALGO), P_("RATIO C", 0, 18, 0, PK_RATC), P_("RATIO A", 0, 35, 0, PK_RATA),
     P_("RATIO B", 0, 360, 0, PK_RATB), P_("HARM", 37, 89, 63, PK_BIP), PN("DETUNE"), PN("FEEDBACK"), PB("MIX")},
    {PN("ATK A"), PN("DEC A"), PN("END A"), PN("LEV A"), PN("ATK B"), PN("DEC B"), PN("END B"), PN("LEV B")},
    {PN("DELAY A"), PO("TRIG A"), PO("RESET A"), PL("PHASE RST", 4, L_PHRT), PN("DELAY B"), PO("TRIG B"),
     PO("RESET B"), PX},
    {PB("OFS C"), PB("OFS A"), PB("OFS B1"), PB("OFS B2"), PX, PN("KEYTRK A"), PN("KEYTRK B1"), PN("KEYTRK B2")},
};
static const par_t PAR_FD[4][8] = {
    {P_("TUNE", 4, 124, 64, PK_BIP), PN("SWP TIME"), PN("SWP DEPTH"), P_("ALGO", 0, 6, 0, PK_ALGO), PN("OP C WAVE"),
     PN("OP AB WAVE"), PN("FEEDBACK"), PN("FOLD")},
    {PN("RATIO A"), PN("DEC A"), PN("END A"), PN("MOD A"), PN("RATIO B"), PN("DEC B"), PN("END B"), PN("MOD B")},
    {PN("HOLD"), PN("DECAY"), P_("PHASE C", 0, 91, 0, PK_PHC), PN("LEVEL"), PX, PX, PO("NOISE RST"), PO("NOISE RM")},
    {PN("NS HOLD"), PN("NS DECAY"), P_("TRANS", 0, 124, 0, PK_NUM), PN("TRANS LEV"), PN("NS BASE"), PN("NS WIDTH"),
     PN("NS GRAIN"), PN("NS LEVEL")},
};
static const par_t PAR_WT[3][8] = {
    {P_("TUNE 1", 4, 124, 64, PK_BIP), P_("WAVE 1", 0, 120, 0, PK_NUM), P_("PD 1", 0, 100, 50, PK_BIP), PN("LEVEL 1"),
     P_("TUNE 2", 4, 124, 64, PK_BIP), P_("WAVE 2", 0, 120, 0, PK_NUM), P_("PD 2", 0, 100, 50, PK_BIP), PN("LEVEL 2")},
    {P_("OFS 1", 0, 100, 50, PK_BIP), PL("TABLE 1", 1, L_TBL), PL("MOD", 3, L_WMOD), PL("RESET", 2, L_RSET),
     P_("OFS 2", 0, 100, 50, PK_BIP), PL("TABLE 2", 1, L_TBL), PX, PN("DRIFT")},
    {PN("NS ATK"), P_("NS HOLD", 0, 127, 0, PK_HOLD), PN("NS DECAY"), PN("NS LEVEL"), PN("NS BASE"), PN("NS WIDTH"),
     PL("NS TYPE", 2, L_NTYP), PN("NS CHAR")},
};
static const par_t PAR_SW[1][8] = {
    {P_("TUNE", 4, 124, 64, PK_BIP), P_("SWRM WAVE", 0, 120, 0, PK_NUM), PN("DETUNE"), PN("MIX"), PL("MAIN OCT", 2, L_MOCT),
     P_("MAIN WAVE", 0, 120, 0, PK_NUM), PN("ANIMATION"), PN("NOISE MOD")},
};
static const uint8_t SYN_PAGES[NENG] = {4, 4, 3, 1};

/* the common pages: what they are, then their parameters (the FLTR machine changes knobs F and G's names) */
enum { PG_FLT1 = 4, PG_FLT2, PG_AMP, PG_FX, PG_MNG };
static const par_t PAR_FLT2[8] = {PN("FLT DELAY"), P_("KEYTRACK", 0, 127, 0, PK_NUM), PN("BASE"), PN("WIDTH"),
                                  PL("BW ROUTE", 1, L_RT), PO("ENV RESET"), PL("FILTER", 5, L_FLT), PX};
static const par_t PAR_AMP[8] = {PN("ATTACK"), P_("HOLD", 0, 127, 0, PK_HOLD), PN("DECAY"), PN("SUSTAIN"),
                                 PN("RELEASE"), PO("RESET"), PL("MODE", 1, L_MODE), PN("VOLUME")};
static const par_t PAR_MNG[8] = {PN("DESTROY"), PL("DST MODE", 3, L_DMODE), PL("SLICE", 16, L_SLICE),
                                 PL("SLICE RATE", 3, L_SRATE), PN("SLC DEPTH"), PN("SLC EDGE"),
                                 P_("SLC SHIFT", 0, 15, 0, PK_NUM), PX};
static const par_t PAR_FX[8] = {PN("BIT REDUCE"), P_("OVERDRIVE", 0, 100, 0, PK_NUM), PN("SRR"), PX,
                                PN("DELAY SEND"), PN("REVERB SEND"), PN("CHORUS SEND"), PL("OD ROUTE", 1, L_RT)};
/* (the FX page's order is the Digitone II's: BR OVER SRR SR.RT DEL REV CHR OD.RT) */
static const uint8_t FX_SLOT[8] = {X_BR, X_OD, X_SRR, X_SPARE, X_DEL, X_REV, X_CHR, X_ODRT};
static const uint8_t AMP_SLOT[8] = {A_ATK, A_HOLD, A_DEC, A_SUS, A_REL, A_RSET, A_MODE, A_VOL};
static const uint8_t FLT2_SLOT[8] = {F_DEL, F_KEYT, F_BASE, F_WDTH, F_BWRT, F_RSET, 0xFF, 0xFE};

/* the pages of a sound: its machine's SYN pages, then FLTR 1, FLTR 2, AMP, FX, MANGLE */
static uint32_t pages_n(const sound_t *s) { return SYN_PAGES[s->mach % NENG] + 5u; }
/* page index p (0 .. pages_n - 1) as a page id: SYN 0 .. 3, or PG_* */
static uint32_t page_id(const sound_t *s, uint32_t p)
{
    uint32_t n = SYN_PAGES[s->mach % NENG];
    return p < n ? p : PG_FLT1 + (p - n);
}
static void page_title(const sound_t *s, uint32_t id, char *b)
{
    static const char *const T[5] = {"FLTR 1", "FLTR 2", "AMP", "FX", "MANGLE"};
    if (id < 4u) {
        str_cpy(b, "SYN  ", 8);
        b[4] = (char)('1' + id);
        b[5] = 0;
    } else
        str_cpy(b, T[id - 4u], 8);
    (void)s;
}
/* FLTR 1, by machine: knobs F and G */
static void flt1_par(const sound_t *s, uint32_t j, par_t *p)
{
    static const par_t A[4] = {PN("ATTACK"), PN("DECAY"), PN("SUSTAIN"), PN("RELEASE")};
    static const par_t FR = PN("FREQ"), EN = PB("ENV DEPTH");
    if (j < 4u)
        *p = A[j];
    else if (j == 4u)
        *p = FR;
    else if (j == 7u)
        *p = EN;
    else {
        static const par_t RS = PN("RESONANCE"), TY = PN("TYPE"), TL = PL("TYPE", 1, L_LEGT), FB = PN("FEEDBACK"),
                           LP = PN("LPF"), GA = PB("GAIN"), Q = PN("Q"), NO = PX;
        switch (s->fmach) {
        case FLT_MM:
            *p = j == 5u ? RS : TY;
            break;
        case FLT_LP4:
            *p = j == 5u ? RS : NO;
            break;
        case FLT_LEG:
            *p = j == 5u ? RS : TL;
            break;
        case FLT_CBM:
        case FLT_CBP:
            *p = j == 5u ? FB : LP;
            break;
        default:
            *p = j == 5u ? GA : Q;
            break;
        }
    }
}
/* parameter j (0 .. 7) of page id: its definition */
static void par_get(const sound_t *s, uint32_t id, uint32_t j, par_t *p)
{
    j &= 7u;
    if (id < 4u) {
        switch (s->mach) {
        case ENG_FMT:
            *p = PAR_FT[id][j];
            break;
        case ENG_FMD:
            *p = PAR_FD[id][j];
            break;
        case ENG_WAV:
            *p = PAR_WT[id < 3u ? id : 2u][j];
            break;
        default:
            *p = PAR_SW[0][j];
            break;
        }
    } else if (id == PG_FLT1)
        flt1_par(s, j, p);
    else if (id == PG_FLT2)
        *p = PAR_FLT2[j];
    else if (id == PG_AMP)
        *p = PAR_AMP[j];
    else if (id == PG_FX)
        *p = PAR_FX[j];
    else
        *p = PAR_MNG[j];
}
/* where it lives in the sound */
static int32_t par_val(const sound_t *s, uint32_t id, uint32_t j)
{
    j &= 7u;
    if (id < 4u)
        return s->syn[id * 8u + j];
    if (id == PG_FLT1)
        return s->flt[j];                                    /* (F_ATK .. F_ENV: 0 .. 7) */
    if (id == PG_FLT2)
        return FLT2_SLOT[j] == 0xFF ? s->fmach : FLT2_SLOT[j] == 0xFE ? 0 : s->flt[FLT2_SLOT[j]];
    if (id == PG_AMP)
        return s->amp[AMP_SLOT[j]];
    if (id == PG_FX)
        return s->fx[FX_SLOT[j]];
    return s->mg[j];
}
static void par_put(sound_t *s, uint32_t id, uint32_t j, int32_t v)
{
    j &= 7u;
    if (id < 4u)
        s->syn[id * 8u + j] = (int16_t)v;
    else if (id == PG_FLT1)
        s->flt[j] = (uint8_t)v;
    else if (id == PG_FLT2) {
        if (FLT2_SLOT[j] == 0xFF)
            s->fmach = (uint8_t)v;
        else if (FLT2_SLOT[j] != 0xFE)
            s->flt[FLT2_SLOT[j]] = (uint8_t)v;
    } else if (id == PG_AMP)
        s->amp[AMP_SLOT[j]] = (uint8_t)v;
    else if (id == PG_FX)
        s->fx[FX_SLOT[j]] = (uint8_t)v;
    else
        s->mg[j] = (uint8_t)v;
}
/* a ratio (x 4) as text: 0.25, 0.5, 0.75, 1, 1.25 .. 16 */
static void ratio_txt(char *b, uint32_t r4)
{
    fmt_int(b, (int32_t)(r4 / 4u));
    if (r4 % 4u) {
        static const char *const Q[4] = {"", ".25", ".5", ".75"};
        if (r4 < 4u)
            str_cpy(b, "0", 4);
        str_cpy(b + str_len(b), Q[r4 % 4u], 4);
    }
}
/* the value as it reads */
static void par_text(const par_t *p, int32_t v, char *b)
{
    b[0] = 0;
    switch (p->kind) {
    case PK_BIP:
        if (v > p->mid)
            str_cpy(b, "+", 2);
        fmt_int(b + str_len(b), v - p->mid);
        break;
    case PK_ALGO:
        fmt_int(b, v + 1);
        break;
    case PK_RATC:
        ratio_txt(b, FT_RC4[clampi(v, 0, 18)]);
        break;
    case PK_RATA:
        ratio_txt(b, FT_RA4[clampi(v, 0, 35)]);
        break;
    case PK_RATB:
        ratio_txt(b, FT_RC4[clampi(v, 0, 360) / 19]);
        str_cpy(b + str_len(b), " / ", 4);
        ratio_txt(b + str_len(b), FT_RC4[clampi(v, 0, 360) % 19]);
        break;
    case PK_ONOFF:
        str_cpy(b, v ? "ON" : "OFF", 4);
        break;
    case PK_LIST:
        str_cpy(b, p->list[clampi(v, 0, p->hi)], 12);
        break;
    case PK_HOLD:
        if (v >= 127)
            str_cpy(b, "NOTE", 6);
        else
            fmt_int(b, v);
        break;
    case PK_PHC:
        if (v >= 91)
            str_cpy(b, "OFF", 4);
        else
            fmt_int(b, v);
        break;
    case PK_NONE:
        break;
    default:
        fmt_int(b, v);
        break;
    }
}
#undef P_
#undef PL
#undef PN
#undef PB
#undef PO
#undef PX
