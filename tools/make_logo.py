#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# BIGORNA: the logo, drawn from geometry (no font): an anvil ("bigorna", Portuguese) as a pictogram of straight
# lines, and the name in thin, squared, widely spaced capitals with small chamfers. One colour: steel.
#   tools/make_logo.py        writes docs/assets/mark.svg, logo-dark.svg, logo-light.svg
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets"

STEEL = "#d2d5da"        # on a dark ground
STEEL_DARK = "#26292e"   # on a light one

H, S, C, W = 60.0, 7.0, 5.0, 44.0    # a letter's height, its stroke, its outer chamfer, its width
CI = 2.0                             # the counters' chamfer
MID = H / 2


def poly(pts):
    return "M" + " L".join(f"{x:g},{y:g}" for x, y in pts) + " Z"


def octagon(x0, y0, x1, y1, c):
    return poly([(x0 + c, y0), (x1 - c, y0), (x1, y0 + c), (x1, y1 - c), (x1 - c, y1), (x0 + c, y1), (x0, y1 - c),
                 (x0, y0 + c)])


def letter(ch):
    """a letter's path (its counters cut, even-odd) and its width"""
    if ch == "I":
        return poly([(0, 0), (S, 0), (S, H), (0, H)]), S
    if ch == "O":
        return octagon(0, 0, W, H, C) + " " + octagon(S, S, W - S, H - S, CI), W
    if ch == "B":
        out = poly([(0, 0), (W - C, 0), (W, C), (W, MID - 3), (W - 3, MID), (W, MID + 3), (W, H - C), (W - C, H),
                    (0, H)])
        return (out + " " + octagon(S, S, W - S, MID - S / 2, CI) + " " +
                octagon(S, MID + S / 2, W - S, H - S, CI)), W
    if ch == "R":
        bowl = poly([(0, 0), (W - C, 0), (W, C), (W, MID + S / 2 - C), (W - C, MID + S / 2), (S, MID + S / 2), (S, H),
                     (0, H)])
        leg = poly([(W / 2 - 2, MID + S / 2), (W / 2 - 2 + S + 2, MID + S / 2), (W, H), (W - S - 2, H)])
        return bowl + " " + octagon(S, S, W - S, MID - S / 2, CI) + " " + leg, W
    if ch == "N":
        return poly([(0, 0), (S, 0), (W - S, H * 0.74), (W - S, 0), (W, 0), (W, H), (W - S, H), (S, H * 0.26),
                     (S, H), (0, H)]), W
    if ch == "A":
        c1, c2 = MID + 1, MID + 1 + S
        out = poly([(C, 0), (W - C, 0), (W, C), (W, H), (W - S, H), (W - S, c2), (S, c2), (S, H), (0, H), (0, C)])
        return out + " " + octagon(S, S, W - S, c1, CI), W
    if ch == "G":
        return poly([(C, 0), (W - C, 0), (W, C), (W, H * 0.28), (W - S, H * 0.28), (W - S, S), (S, S), (S, H - S),
                     (W - S, H - S), (W - S, MID + S / 2), (W / 2 + 2, MID + S / 2), (W / 2 + 2, MID - S / 2),
                     (W, MID - S / 2), (W, H - C), (W - C, H), (C, H), (0, H - C), (0, C)]), W
    raise ValueError(ch)


def word(text, gap=17.0):
    """the word's paths, each letter moved into place; its width"""
    x, parts = 0.0, []
    for ch in text:
        d, w = letter(ch)
        parts.append(f'<path transform="translate({x:g} 0)" d="{d}"/>')
        x += w + gap
    return "\n    ".join(parts), x - gap


# the anvil, side on, straight lines only: the horn tapering left, the face, the heel, the waist, the feet
ANVIL = poly([(0, 13), (34, 6), (112, 6), (112, 20), (86, 22), (79, 38), (94, 47), (94, 54), (34, 54), (34, 47),
               (49, 38), (42, 22), (22, 20)])
AW, AH = 112, 60                     # its box (the feet at 54, the word's baseline)


def mark():
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -4 {AW + 8} {AH + 8}" width="{AW + 8}" height="{AH + 8}">
  <title>Bigorna</title>
  <path fill="{STEEL}" d="{ANVIL}"/>
</svg>
"""


def logo(steel):
    paths, w = word("BIGORNA")
    lx = AW + 34                                   # the word, after the anvil
    total = lx + w
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {total:g} {H:g}" width="{total:g}" height="{H:g}">
  <title>Bigorna</title>
  <path fill="{steel}" transform="translate(0 0)" d="{ANVIL}"/>
  <g transform="translate({lx:g} 0)" fill="{steel}" fill-rule="evenodd">
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
