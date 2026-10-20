#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# BIGORNA: the logo, drawn from geometry (no font): an anvil ("bigorna", Portuguese), its face hot, and the name in
# squared, chamfered capitals, as letters cut from plate.
#   tools/make_logo.py        writes docs/assets/mark.svg, logo-dark.svg, logo-light.svg
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets"

STEEL = "#c8ccd2"        # the letters and the anvil on a dark ground
STEEL_DARK = "#2a2d32"   # .. on a light one
HOT = "#ff6a14"          # the anvil's face: hot metal

H, S, C, W = 100, 24, 12, 70   # a letter's height, its stroke, its chamfer, its width (I: the stroke)


def poly(pts):
    return "M" + " L".join(f"{x:g},{y:g}" for x, y in pts) + " Z"


def octagon(x0, y0, x1, y1, c):
    return poly([(x0 + c, y0), (x1 - c, y0), (x1, y0 + c), (x1, y1 - c), (x1 - c, y1), (x0 + c, y1), (x0, y1 - c),
                 (x0, y0 + c)])


def letter(ch):
    """a letter's path, in a box W (or S) by H, its counters cut (even-odd)"""
    if ch == "I":
        return octagon(0, 0, S, H, 5), S
    if ch == "O":
        return octagon(0, 0, W, H, C) + " " + octagon(S, S, W - S, H - S, 5), W
    if ch == "B":
        out = poly([(0, 0), (W - C, 0), (W, C), (W, 44), (W - 6, 50), (W, 56), (W, H - C), (W - C, H), (0, H)])
        return out + " " + poly([(S, S), (W - S, S), (W - S, 38), (S, 38)]) + " " + \
            poly([(S, 62), (W - S, 62), (W - S, H - S), (S, H - S)]), W
    if ch == "R":
        out = poly([(0, 0), (W - C, 0), (W, C), (W, 50), (W - 8, 58), (W, 66), (W, H), (W - S, H), (W - S, 68),
                    (W - S - 6, 62), (S, 62), (S, H), (0, H)])
        return out + " " + poly([(S, S), (W - S, S), (W - S, 40), (S, 40)]), W
    if ch == "N":
        return poly([(0, 0), (S + 4, 0), (W - S, 62), (W - S, 0), (W, 0), (W, H), (W - S - 4, H), (S, 38), (S, H),
                     (0, H)]), W
    if ch == "A":
        out = poly([(C, 0), (W - C, 0), (W, C), (W, H), (W - S, H), (W - S, 66), (S, 66), (S, H), (0, H), (0, C)])
        return out + " " + poly([(S, S), (W - S, S), (W - S, 46), (S, 46)]), W
    if ch == "G":
        return poly([(C, 0), (W - C, 0), (W, C), (W, 30), (W - S, 30), (W - S, S), (S, S), (S, H - S), (W - S, H - S),
                     (W - S, 62), (36, 62), (36, 44), (W, 44), (W, H - C), (W - C, H), (C, H), (0, H - C),
                     (0, C)]), W
    raise ValueError(ch)


def word(text, gap=14):
    """the word's paths, each letter moved into place; its width"""
    x, parts = 0, []
    for ch in text:
        d, w = letter(ch)
        parts.append(f'<path transform="translate({x} 0)" d="{d}"/>')
        x += w + gap
    return "\n    ".join(parts), x - gap


# the anvil, side on: the horn to the left, the face, the heel with its hardy and pritchel holes, the waist, the feet
ANVIL = ("M6,46 Q30,30 64,28 L176,28 L192,28 L196,32 L196,58 L178,62 Q152,64 146,72 L142,94 Q146,104 172,110 "
         "L184,116 L184,126 L36,126 L36,116 L48,110 Q74,104 78,94 L74,72 Q70,64 58,62 Q28,60 6,46 Z "
         "M166,33 L176,33 L176,43 L166,43 Z M157.4,38 A3.4,3.4 0 1,0 150.6,38 A3.4,3.4 0 1,0 157.4,38 Z")
FACE = '<path d="M64,28 L192,28 L196,32 L66,32 Q65,30 64,28 Z" fill="{hot}"/>'


def anvil_group(steel, hot):
    return f'<g fill="{steel}" fill-rule="evenodd"><path d="{ANVIL}"/></g>' + FACE.format(hot=hot)


def mark():
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 132" width="200" height="132">
  <title>Bigorna</title>
  {anvil_group(STEEL, HOT)}
</svg>
"""


def logo(steel):
    paths, w = word("BIGORNA")
    lx = 230                                       # the word, to the right of the anvil
    total = lx + w
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {total} 132" width="{total}" height="132">
  <title>Bigorna</title>
  <g fill="{steel}" fill-rule="evenodd"><path d="{ANVIL}"/></g>
  {FACE.format(hot=HOT)}
  <g transform="translate({lx} 16)" fill="{steel}" fill-rule="evenodd">
    {paths}
  </g>
  <rect x="{lx}" y="122" width="{w}" height="6" fill="{HOT}"/>
</svg>
"""


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "mark.svg").write_text(mark())
    (OUT / "logo-dark.svg").write_text(logo(STEEL))         # for a dark ground
    (OUT / "logo-light.svg").write_text(logo(STEEL_DARK))   # for a light one
    print("logo: docs/assets/mark.svg, logo-dark.svg, logo-light.svg")
