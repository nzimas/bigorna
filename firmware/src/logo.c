/* SPDX-License-Identifier: GPL-3.0-only
 * BIGORNA: the logo on the splash screen -- the anvil over the lettering, in steel on black, the build under them.
 * The two are 4-bit alpha bitmaps made by tools/gen_logo.py from the website logo's shapes (fw_logo.h). */
#include "fw_logo.h"

#define LOGO_STEEL RGB(0xD2, 0xD5, 0xDA)

/* a bitmap w x h, centred across the screen, its top at y; c over black */
static void lg_draw(uint32_t y, const uint8_t *bm, uint32_t w, uint32_t h, uint16_t c)
{
    uint16_t ramp[16];
    int32_t r = c >> 11, g = (c >> 5) & 63, b = c & 31, a;
    uint32_t x0 = (240u - w) / 2u, bpr = (w + 1u) / 2u;
    for (a = 0; a < 16; a++)
        ramp[a] = swap16((uint32_t)(((r * a / 15) << 11) | ((g * a / 15) << 5) | (b * a / 15)));
    CV_AREA(0, y, 240, h, C_BLACK) {
        uint32_t i, j;
        for (j = 0; j < cv_h; j++) {
            const uint8_t *row = bm + ((uint32_t)cv_oy + j) * bpr;
            for (i = 0; i < w; i++) {
                uint32_t v = row[i / 2u];
                v = (i & 1u) ? (v & 15u) : (v >> 4);
                if (v)
                    cv_px[j * cv_w + x0 + i] = ramp[v];
            }
        }
    }
}

static void splash_draw(const char *build)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    lg_draw(30, LOGO_ANVIL, LOGO_ANVIL_W, LOGO_ANVIL_H, LOGO_STEEL);
    lg_draw(132, LOGO_WORD, LOGO_WORD_W, LOGO_WORD_H, LOGO_STEEL);
    draw_text_box(0, 188, 240, &FONT_S, build, C_GREY, 1);
}
