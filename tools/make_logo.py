#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# BIGORNA: the logo -- an anvil ("bigorna", Portuguese) as a pictogram of straight lines, and the name set in
# Michroma (SIL OFL 1.1, assets/fonts/Michroma-Regular.ttf), its outlines turned into paths so the SVG needs no
# font. One colour: steel.
#   .venv/bin/python tools/make_logo.py        writes docs/assets/mark.svg, logo-dark.svg, logo-light.svg
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets"
FONT = ROOT / "assets" / "fonts" / "Michroma-Regular.ttf"

STEEL = "#d2d5da"        # on a dark ground
STEEL_DARK = "#26292e"   # on a light one

CAP = 60.0               # the capitals' height, in the logo's units
TRACK = 0.14             # letter spacing, in ems (as the wordmark chosen)
GAP = 34.0               # between the anvil and the word
STROKE = 7.5             # the anvil drawn in outline, as thick as Michroma's stems (192 of 1536 units to the cap)
PAD = STROKE / 2         # (the outline's half outside the shape)

# the anvil, side on, straight lines only: the horn tapering left, the face, the heel, the waist, the feet
ANVIL = "M0,13 L34,6 L112,6 L112,20 L86,22 L79,38 L94,47 L94,54 L34,54 L34,47 L49,38 L42,22 L22,20 Z"
A_TOP, A_BOT, A_W = 6.0, 54.0, 112.0
A_S = CAP / (A_BOT - A_TOP)             # (its face at the capitals' top, its feet on their baseline)


def word(text):
    """the word's glyphs as paths (y down, the capitals' top at 0); its width"""
    f = TTFont(FONT)
    gs, cmap, hmtx = f.getGlyphSet(), f.getBestCmap(), f["hmtx"]
    upem, cap = f["head"].unitsPerEm, f["OS/2"].sCapHeight
    s = CAP / cap
    x, parts = 0.0, []
    for i, ch in enumerate(text):
        g = cmap[ord(ch)]
        pen = SVGPathPen(gs)
        gs[g].draw(pen)
        parts.append(f'<path transform="translate({x:.2f} {CAP:g}) scale({s:.6f} {-s:.6f})" d="{pen.getCommands()}"/>')
        x += hmtx[g][0] * s
        if i + 1 < len(text):
            x += TRACK * upem * s
    return "\n    ".join(parts), x


def anvil(steel):
    return (f'<path fill="none" stroke="{steel}" stroke-width="{STROKE / A_S:.3f}" stroke-linejoin="miter" '
            f'transform="translate({PAD:g} {-A_TOP * A_S:.2f}) scale({A_S:.4f})" d="{ANVIL}"/>')


def mark():
    w, h = A_W * A_S + 2 * PAD, CAP
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 {-PAD - 4:g} {w + 8:.1f} {h + 2 * PAD + 8:.1f}" width="{w + 8:.0f}" height="{h + 2 * PAD + 8:.0f}">
  <title>Bigorna</title>
  {anvil(STEEL)}
</svg>
"""


def logo(steel):
    paths, w = word("BIGORNA")
    lx = A_W * A_S + 2 * PAD + GAP
    total = lx + w
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 {-PAD:g} {total:.1f} {CAP + 2 * PAD:g}" width="{total:.0f}" height="{CAP + 2 * PAD:g}">
  <title>Bigorna</title>
  {anvil(steel)}
  <g transform="translate({lx:.2f} 0)" fill="{steel}">
    {paths}
  </g>
</svg>
"""


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "mark.svg").write_text(mark())
    (OUT / "logo-dark.svg").write_text(logo(STEEL))         # for a dark ground
    (OUT / "logo-light.svg").write_text(logo(STEEL_DARK))   # for a light one
    print("logo: docs/assets/mark.svg, logo-dark.svg, logo-light.svg")
