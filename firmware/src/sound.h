/* SPDX-License-Identifier: GPL-3.0-only
 * The sound of a track, in the units of the Digitone II's own parameters (0 .. 127 mostly, bipolar ones centred
 * on 64), so that what the screen shows and what the patch generator draws read as on the original:
 *   the SYN machine and its 32 parameters (SYN pages 1 .. 4, knobs A .. H), the FLTR machine and its page, the
 *   base-width filter, the AMP page and the FX page.
 * The main loop owns sound_t; it cooks a sound into what the voices read (ck_t, voice.c) and publishes it. */
#pragma once

enum { ENG_FMT, ENG_FMD, ENG_WAV, ENG_SWM, NENG };     /* FM TONE, FM DRUM, WAVETONE, SWARMER */
enum { FLT_MM, FLT_LP4, FLT_LEG, FLT_CBM, FLT_CBP, FLT_EQ, NFLT };   /* MULTI-MODE, LOWPASS 4, LEGACY LP/HP, COMB-, COMB+, EQUALIZER */

/* FLTR pages 1 and 2 */
enum { F_ATK, F_DEC, F_SUS, F_REL, F_FREQ, F_RESO, F_TYPE, F_ENV, F_DEL, F_KEYT, F_BASE, F_WDTH, F_BWRT, F_RSET, NF };
/* AMP page */
enum { A_ATK, A_HOLD, A_DEC, A_SUS, A_REL, A_RSET, A_MODE, A_PAN, A_VOL, NA };
/* FX page (the routing values: 0 PRE, 1 POST) */
enum { X_BR, X_OD, X_SRR, X_GRAIN, X_ODRT, X_DEL, X_REV, X_CHR, NX };   /* (X_GRAIN: signed, -63 .. 63; it was SRR's
                                                                      * route, 0 PRE / 1 POST, until build 17) */

/* SYN parameters, by machine (page-major: page 1 knob A = 0 .. page 4 knob H = 31) */
enum {   /* FM TONE */
    FT_ALGO, FT_RATC, FT_RATA, FT_RATB, FT_HARM, FT_DTUN, FT_FDBK, FT_MIX,
    FT_AATK, FT_ADEC, FT_AEND, FT_ALEV, FT_BATK, FT_BDEC, FT_BEND, FT_BLEV,
    FT_ADEL, FT_ATRG, FT_ARST, FT_PHRT, FT_BDEL, FT_BTRG, FT_BRST, FT_P3H,
    FT_OFSC, FT_OFSA, FT_OFSB1, FT_OFSB2, FT_P4E, FT_KTA, FT_KTB1, FT_KTB2
};
enum {   /* FM DRUM */
    FD_TUNE, FD_STIM, FD_SDEP, FD_ALGO, FD_OPC, FD_OPAB, FD_FDBK, FD_FOLD,
    FD_RATA, FD_DECA, FD_ENDA, FD_MODA, FD_RATB, FD_DECB, FD_ENDB, FD_MODB,
    FD_HOLD, FD_DEC, FD_PHC, FD_LEV, FD_P3E, FD_P3F, FD_NRST, FD_NRM,
    FD_NHLD, FD_NDEC, FD_TRAN, FD_TLEV, FD_NBAS, FD_NWID, FD_GRAN, FD_NLEV
};
enum {   /* WAVETONE */
    WT_TUN1, WT_WAV1, WT_PD1, WT_LEV1, WT_TUN2, WT_WAV2, WT_PD2, WT_LEV2,
    WT_OFS1, WT_TBL1, WT_MOD, WT_RSET, WT_OFS2, WT_TBL2, WT_P2G, WT_DRIF,
    WT_NATK, WT_NHLD, WT_NDEC, WT_NLEV, WT_NBAS, WT_NWID, WT_NTYP, WT_CHAR
};
enum {   /* SWARMER */
    SW_TUNE, SW_SWRM, SW_DET, SW_MIX, SW_MOCT, SW_MAIN, SW_ANIM, SW_NMOD
};

typedef struct {
    uint8_t mach;                /* ENG_* */
    uint8_t fmach;               /* FLT_* */
    int16_t syn[32];             /* (FM TONE's RATIO B is 0 .. 360: B1 x 19 + B2) */
    uint8_t flt[NF];
    uint8_t amp[NA];
    uint8_t fx[NX];
    uint8_t family;              /* the recipe it was drawn from (gen.c), for SHIFT + HOME */
    uint32_t seed;
} sound_t;

/* one parameter's place on a page, its range, and its centre (bipolar parameters show value - centre) */
typedef struct { const char *name; int16_t lo, hi, mid; } pdef_t;
