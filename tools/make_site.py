#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# BIGORNA (as TONITOR's): the website's generated parts, in docs/ (GitHub Pages):
#   docs/index.html, docs/install.html, docs/projects.html, docs/apps.html   site/*.html in the common frame (the home page's demo
#                        pattern: docs/assets/demo/, tools/render_demo.mjs then tools/make_loop.py; its player js/demo.js)
#   docs/manual.html     the README's manual (from "# Manual" to "# Building from source")
#   docs/changelog.html  CHANGELOG.md
#   docs/firmware/       the package the browser installer writes, and latest.json (its file, version, SHA-256)
#   tools/make_site.py [PACKAGE.fwsc]        (default build/bigorna.fwsc; run tools/build.py first)
# The logo: tools/make_logo.py (docs/assets/); the stylesheet and scripts: docs/css, docs/js, written by hand.
# The markdown is the little the README uses: headings, paragraphs, lists, tables, quotes, code, emphasis, links.
import hashlib
import html
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
REPO = "https://github.com/nzimas/bigorna"
# the stylesheet's address changes with it, so browsers never keep an old one
CSS_V = hashlib.sha256((Path(__file__).resolve().parent.parent / "docs/css/site.css").read_bytes()).hexdigest()[:10]
MANUAL_JS_V = hashlib.sha256((Path(__file__).resolve().parent.parent / "docs/js/manual.js").read_bytes()).hexdigest()[:10]
LOGO_V = hashlib.sha256((Path(__file__).resolve().parent.parent / "docs/assets/logo-dark.svg").read_bytes()).hexdigest()[:10]   # (the images are cached too)


def slug(text):
    s = re.sub(r"<[^>]+>", "", text).lower()
    s = re.sub(r"[^\w\s-]", "", s, flags=re.U)
    return re.sub(r"\s+", "-", s.strip())


def inline(t):
    codes = []

    def keep(m):
        codes.append(f"<code>{html.escape(m[1])}</code>")
        return f"\0{len(codes) - 1}\0"
    t = re.sub(r"`([^`]+)`", keep, t)
    if not re.search(r"<[a-z/][^>]*>", t):
        t = html.escape(t, quote=False)

    def link(m):
        url = m[2]
        if not re.match(r"[a-z]+:|#", url):        # a file in the repository
            url = f"{REPO}/blob/main/{url}"
        return f'<a href="{html.escape(url)}">{m[1]}</a>'
    t = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", link, t)
    t = re.sub(r"<(https?://[^>]+)>", r'<a href="\1">\1</a>', t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"(?<![\w*])\*([^*\s][^*]*?)\*(?![\w*])", r"<i>\1</i>", t)
    t = t.replace(" -- ", " — ").replace(" --\n", " —\n")      # (the changelog's dashes)
    return re.sub(r"\0(\d+)\0", lambda m: codes[int(m[1])], t)


def to_html(md, toc):
    out, lines, i = [], md.splitlines(), 0

    def para(buf):
        if buf:
            out.append("<p>" + inline(" ".join(buf)) + "</p>")
            buf.clear()
    buf = []
    while i < len(lines):
        ln = lines[i]
        if not ln.strip():
            para(buf)
            i += 1
        elif ln.startswith("```"):
            para(buf)
            j = i + 1
            while j < len(lines) and not lines[j].startswith("```"):
                j += 1
            out.append("<pre><code>" + html.escape("\n".join(lines[i + 1:j])) + "</code></pre>")
            i = j + 1
        elif m := re.match(r"(#{1,4})\s+(.*)", ln):
            para(buf)
            lv, text = len(m[1]), m[2]
            sid = slug(text)
            out.append(f'<h{lv} id="{sid}">{inline(text)}</h{lv}>')
            if lv in (2, 3):
                toc.append((lv, sid, re.sub(r"<[^>]+>", "", inline(text))))
            i += 1
        elif ln.startswith("|"):
            para(buf)
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                cells = [c.strip() for c in re.split(r"(?<!\\)\|", lines[i].strip())[1:-1]]
                if not all(re.fullmatch(r":?-+:?", c) for c in cells):
                    rows.append(cells)
                i += 1
            t = ["<table>", "<tr>" + "".join(f"<th>{inline(c)}</th>" for c in rows[0]) + "</tr>"]
            t += ["<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>" for r in rows[1:]]
            out.append("\n".join(t + ["</table>"]))
        elif ln.startswith(">"):
            para(buf)
            q = []
            while i < len(lines) and lines[i].startswith(">"):
                q.append(lines[i][1:].strip())
                i += 1
            q = [x for x in q if not re.fullmatch(r"\[!\w+\]", x)]
            out.append("<blockquote><p>" + inline(" ".join(q)) + "</p></blockquote>")
        elif re.match(r"\s*[-*] ", ln):
            para(buf)
            items, depth0 = [], len(ln) - len(ln.lstrip())
            while i < len(lines) and (re.match(r"\s*[-*] ", lines[i]) or (lines[i].startswith("  ") and lines[i].strip())):
                d = len(lines[i]) - len(lines[i].lstrip())
                if re.match(r"\s*[-*] ", lines[i]):
                    items.append([d > depth0, lines[i].strip()[2:]])
                else:
                    items[-1][1] += " " + lines[i].strip()
                i += 1
            h, open_sub = ["<ul>"], False
            for sub, text in items:
                if sub and not open_sub:
                    h[-1] = h[-1].removesuffix("</li>")
                    h.append("<ul>")
                    open_sub = True
                elif not sub and open_sub:
                    h.append("</ul></li>")
                    open_sub = False
                h.append(f"<li>{inline(text)}</li>")
            if open_sub:
                h.append("</ul></li>")
            out.append("\n".join(h + ["</ul>"]))
        elif ln.lstrip().startswith("<"):
            para(buf)
            out.append(ln)
            i += 1
        else:
            buf.append(ln.strip())
            i += 1
    para(buf)
    return "\n".join(out)


def page(title, current, body, desc, head=""):
    """every page: the menu down the left (above the content on a narrow screen), the content, the footer"""
    nav = [("./", "Home"), ("screens.html", "Screens"), ("install.html", "Install"), ("backup.html", "Backup"), ("manual.html", "Manual"),
           ("changelog.html", "Changelog"), ("projects.html", "Other projects"), ("apps.html", "Apps"),
           (REPO, "GitHub")]
    links = "\n      ".join(f'<a href="{h}"' + (' aria-current="page"' if n == current else "") + f">{n}</a>"
                           for h, n in nav)
    return f"""<!doctype html>
<!-- SPDX-License-Identifier: MIT -- Bigorna website (generated by tools/make_site.py from site/, README.md, CHANGELOG.md) -->
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<meta name="description" content="{desc}">
<link rel="icon" href="assets/mark.svg?v={LOGO_V}" type="image/svg+xml">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Atkinson+Hyperlegible:wght@400;700&family=Big+Shoulders+Display:wght@700;800&display=swap" rel="stylesheet">
<link rel="stylesheet" href="css/site.css?v={CSS_V}">{head}
</head>
<body>
<div class="layout">
  <aside class="side">
    <div class="top">
      <a class="brand" href="./"><img src="assets/logo-dark.svg?v={LOGO_V}" alt="Bigorna" width="780" height="68"></a>
      <button class="menu-btn" type="button" aria-label="Menu" aria-controls="site-nav" aria-expanded="false"><span></span><span></span><span></span></button>
    </div>
    <nav id="site-nav" aria-label="Site">
      <button class="menu-close" type="button" aria-label="Close the menu">&times;</button>
      {links}
    </nav>
    <div class="menu-shade" hidden></div>
    <form class="donate donate-side" action="https://www.paypal.com/cgi-bin/webscr" method="post" target="_top">
      <input type="hidden" name="cmd" value="_xclick">
      <input type="hidden" name="business" value="nzimas.lv@gmail.com">
      <input type="hidden" name="item_name" value="Support Bigorna">
      <input type="hidden" name="currency_code" value="EUR">
      <input type="image" src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" name="submit"
             title="PayPal - The safer, easier way to pay online!" alt="Donate with PayPal button">
    </form>
  </aside>
  <div class="content">
    <main>
{body}
    </main>
    <footer>
      <p>Developed by Nuno Zimas. Firmware: GPL-3.0. Website and logo: MIT. <a href="{REPO}">Source on GitHub</a>.</p>
      <p>M-VAVE and FM-1 are trademarks of their owner; Elektron and Digitone are trademarks of Elektron Music Machines.
        They are used only to say which device Bigorna runs on and which instrument it takes as its model.</p>
      <form class="donate donate-foot" action="https://www.paypal.com/cgi-bin/webscr" method="post" target="_top">
        <input type="hidden" name="cmd" value="_xclick">
        <input type="hidden" name="business" value="nzimas.lv@gmail.com">
        <input type="hidden" name="item_name" value="Support Bigorna">
        <input type="hidden" name="currency_code" value="EUR">
        <input type="image" src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" name="submit"
               title="PayPal - The safer, easier way to pay online!" alt="Donate with PayPal button">
      </form>
    </footer>
  </div>
</div>
<script src="js/menu.js?v={CSS_V}"></script>
</body>
</html>
"""


def fragments():
    src = ROOT / "site"
    (DOCS / "index.html").write_text(page(
        "Bigorna — a firmware for the M-VAVE FM-1", "Home", (src / "home.html").read_text().replace("{LOGO_V}", LOGO_V),
        "Bigorna: a firmware for the M-VAVE FM-1 for rhythmic noise, power noise, industrial and IDM, after the "
        "Elektron Digitone II.",
        f'\n<script src="js/demo.js?v={hashlib.sha256((DOCS / "js" / "demo.js").read_bytes()).hexdigest()[:10]}" defer></script>'))
    (DOCS / "screens.html").write_text(page(
        "Bigorna on the FM-1's screen", "Screens", (src / "screens.html").read_text(),
        "Bigorna on the M-VAVE FM-1's screen: the track view, the sound's pages, the step view, the effects, the "
        "patterns, CHORD mode and the looper."))
    (DOCS / "install.html").write_text(page(
        "Install Bigorna", "Install", (src / "install.html").read_text(),
        "Install Bigorna on an M-VAVE FM-1 from your browser, over USB MIDI.",
        '\n<script type="module" src="js/install.js"></script>'))
    (DOCS / "backup.html").write_text(page(
        "Back up Bigorna sessions", "Backup", (src / "backup.html").read_text(),
        "Save the sessions on an M-VAVE FM-1 running Bigorna to a file, and restore them, from your browser.",
        '\n<script type="module" src="js/backup.js"></script>'))
    (DOCS / "projects.html").write_text(page(
        "Other projects by Nuno Zimas", "Other projects", (src / "projects.html").read_text(),
        "Firmware and instruments by Nuno Zimas for the M-VAVE FM-1, the Elektron Model:Cycles and Digitone II, the "
        "Ableton Move, the Akai MPC One and macOS."))
    (DOCS / "apps.html").write_text(page(
        "Apps by Nuno Zimas", "Apps", (src / "apps.html").read_text(),
        "Music apps by Nuno Zimas for phones and tablets: Vascular, a touch instrument for iOS and Android."))


# The FM-1's face, in the units of Felucca's browser emulator (hugelton/Felucca, web/emu/index.html, its "wide"
# layout, 1000 x 624; GR!MM's page uses the same): the positions only, drawn here and numbered for the table.
FM1_KNOBS = {"MASTER": (105, 100, 24), "SELECT": (215, 100, 24), "PRESETS": (105, 205, 24), "ALGORITHM": (215, 205, 24),
             "KNOB 1": (610, 100, 22), "KNOB 2": (714, 100, 22), "KNOB 3": (818, 100, 22), "KNOB 4": (922, 100, 22)}
FM1_OCT = [(60, 258, 90, 34), (170, 258, 90, 34)]
FM1_GRID = (582, 174, 54, 48, 63, 62)            # the 2 x 6 buttons: x, y, w, h, step x, step y
FM1_KEYBED = (36, 332, 928, 256)
FM1_BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]   # the white key each black one follows
FM1_COLOURS = {"body": "#1d1d1f", "tray": "#141415", "part": "#2a2a2d", "edge": "#45454a"}

# the table under the drawings: (numbers, control, what it does under Bigorna); a row of several numbers spans them
CONTROLS = [
    ([1], "MASTER", "The output level."),
    ([2], "SELECT", "The chosen track's level. On the SCALE page: the root. A step held: its level, locked."),
    ([3], "PRESETS", "The chosen track's machine. <b>SHIFT +</b>: a new sound of the family before or after. On a page: "
                     "its rows, slots or entries; on the EDIT and SEQ pages, turned right, rows E to H."),
    ([4], "ALGORITHM", "The chosen track's pan. On the SCALE page: the scale. A step held: its pan, locked."),
    ([5], "OCT−", "The chosen track's octave down. In the step view: its "
                  '<a href="#transposition">transposition</a>. <b>SHIFT +</b>: every track\'s transposition. '
                  "On the EDIT and SEQ pages: the page before."),
    ([6], "OCT+", "The same, up."),
    ([7], "the screen", 'See <a href="#the-screen">The screen</a>.'),
    ([8], "KNOB 1", "The chosen track's filter FREQ. On a page: row A."),
    ([9], "KNOB 2", "The chosen track's filter RESO. On a page: row B."),
    ([10], "KNOB 3", "The chosen track's delay send. <b>SHIFT +</b>: its chorus send. On a page: row C."),
    ([11], "KNOB 4", "The chosen track's reverb send. <b>SHIFT +</b>: its overdrive. On a page: row D."),
    ([12, 13, 14, 15, 16, 17], ["FX (T1)", "SEL (T2)", "ENV (T3)", "LFO (T4)", "EDIT (T5)", "GLO (T6)"],
     "<b>T1</b> to <b>T6</b>: choose track 1 to 6. From any page, back to the track. <b>Held + white key 1–4</b>, "
     'on DRUM: its <a href="#subtracks">subtrack</a> ST-A to ST-D (<b>SHIFT +</b>: give one up). In '
     '<a href="#mute-mode">MUTE mode</a>: mute; <b>held</b> (1 s), solo.'),
    ([18], "HOME", '<a href="#sounds">A new sound</a> for the chosen track. <b>SHIFT +</b>: of the same family. '
                   "<b>Held</b> (0.8 s): save the session."),
    ([19], "SAVE", 'The <a href="#patterns">PATTERNS</a> grid. <b>Held</b> (0.8 s): save the session (on the grid: the '
                   'pattern playing, into the slot under the cursor). <b>SHIFT +</b>: the <a href="#sessions">SESSIONS</a> '
                   'list; <b>held</b>: <a href="#global-setup">GLOBAL SETUP</a>.'),
    ([20], "ARP", 'The <a href="#scales">SCALE</a> page. <b>SHIFT +</b>: a scale and root at random.'),
    ([21], "SEQ", 'The <a href="#the-effects-seq">effects\' pages</a>, then the chosen track\'s LFOs. '
                  "<b>SHIFT +</b>: its LFOs drawn anew."),
    ([22], "PLAY", 'Start or stop the <a href="#the-sequencer">sequencer</a>. <b>Held</b> (0.8 s): the TEMPO page. '
                   "<b>SHIFT +</b>: the metronome."),
    ([23], "REC", '<a href="#recording">Record</a> into the steps. <b>SHIFT +</b>: the <a href="#the-looper">looper</a>; '
                  "<b>held</b>, the track's loops."),
    ([24], "black key 1", '<a href="#chord-mode">CHORD mode</a>; <b>SHIFT +</b>, the next form. In the step view: '
                          'a new <a href="#phrases">phrase</a>. With SHIFT held on the PATTERNS grid: empty a pattern.'),
    ([25], "black key 2", '<a href="#mute-mode">MUTE mode</a>. In the step view: the TRACK page.'),
    ([26], "black key 3", '<b>Held</b>: <a href="#break">BREAK</a>; <b>SHIFT +</b>, on every track. In the step view: '
                          "TRIG."),
    ([27], "black key 4", '<a href="#midi-learn">MIDI LEARN</a>. In the step view: MOD.'),
    ([28], "black key 5", 'The <a href="#arpeggiator">arpeggiator</a>; <b>held</b> (0.8 s), its page.'),
    ([29, 30, 31], ["black key 6", "black key 7", "black key 8"],
     "In the step view: the track's direction, REVERSE, PING-PONG, RANDOM. In CHROMATIC: notes."),
    ([32], "black key 9", '<b>STEP</b>: the <a href="#the-step-view">step view</a>. <b>SHIFT +</b>: empty the '
                          "chosen track's steps."),
    ([33], "black key 10", '<b>EDIT</b>: the <a href="#the-sounds-pages-edit">sound\'s pages</a> (not the EDIT button, T5).'),
    ([34], "black key 11", "<b>SHIFT</b>."),
    ([35], "the 16 white keys", "Play the chosen track (its subtrack chosen) in the scale; the root's keys glow. In the "
                                "step view: the 16 steps. On the PATTERNS grid: the 16 patterns. With a DRUM track's "
                                "button held, 1–4: its subtracks."),
]

# KNOB 1-4 on each page
KNOB_PAGES = [
    ("the track", "filter FREQ", "filter RESO", "delay send", "reverb send"),
    ("SHIFT held", "", "", "chorus send", "overdrive"),
    ('<a href="#the-sounds-pages-edit">EDIT</a>', "row A (E)", "row B (F)", "row C (G)", "row D (H)"),
    ('<a href="#the-effects-seq">SEQ</a>', "row A (E)", "row B (F)", "row C (G)", "row D (H)"),
    ('a step held, <a href="#a-steps-own-pages">TRIG</a> on', "note", "velocity", "length", "condition"),
    ('a step held, <a href="#a-steps-own-pages">MOD</a> on', "MUTATE", "MUT EVERY", "MIDI FX", "FX EVERY"),
]

def _svg_callouts(calls):
    """numbered leader lines: each a path from the control (a dot) to its number (a circle, a link to its row)"""
    out = []
    for n, pts in calls:
        d = " ".join(f"{x:g},{y:g}" for x, y in pts)
        (x0, y0), (x1, y1) = pts[0], pts[-1]
        out.append(f'<a href="#ctl-{n}" class="callout" data-n="{n}" aria-label="{n}">'
                   f'<polyline points="{d}" class="lead"/><circle cx="{x0:g}" cy="{y0:g}" r="4.5" class="tip"/>'
                   f'<circle cx="{x1:g}" cy="{y1:g}" r="17" class="num"/>'
                   f'<text x="{x1:g}" y="{y1 + 6.5:g}">{n}</text></a>')
    return "\n".join(out)


def panel_svg():
    """the face above the keys, its controls numbered 1-23"""
    c = FM1_COLOURS
    s = [f'<path d="M4,318 V44 A40,40 0 0 1 44,4 H956 A40,40 0 0 1 996,44 V318 Z" fill="{c["body"]}"/>',
         '<line x1="4" y1="318" x2="996" y2="318" class="cut"/>',
         f'<rect x="50" y="250" width="220" height="50" rx="14" fill="{c["tray"]}"/>',
         f'<rect x="568" y="160" width="400" height="140" rx="14" fill="{c["tray"]}"/>',
         '<rect x="292" y="44" width="256" height="256" rx="18" fill="#0b0b0c"/>',
         f'<rect x="300" y="52" width="240" height="240" rx="4" fill="#141418" stroke="{c["edge"]}"/>']
    for x, y, r in FM1_KNOBS.values():
        s.append(f'<circle cx="{x}" cy="{y}" r="{r + 5}" fill="{c["tray"]}"/>'
                 f'<circle cx="{x}" cy="{y}" r="{r}" fill="{c["part"]}" stroke="{c["edge"]}" stroke-width="2"/>'
                 f'<line x1="{x}" y1="{y - r + 5}" x2="{x}" y2="{y - r + 13}" stroke="#d6d6d8" stroke-width="3" stroke-linecap="round"/>')
    gx, gy, gw, gh, dx, dy = FM1_GRID
    rects = list(FM1_OCT) + [(gx + i * dx, gy + j * dy, gw, gh) for j in (0, 1) for i in range(6)]
    for x, y, w, h in rects:
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="{c["part"]}" stroke="{c["edge"]}" stroke-width="1.5"/>')
    k = FM1_KNOBS
    calls = [(1, [(k["MASTER"][0] - 29, 100), (-40, 100)]),
             (2, [(215, 100 - 29), (215, -40)]),
             (3, [(105 - 29, 205), (-40, 205)]),
             (4, [(194, 185), (160, 152), (-40, 152)]),
             (5, [(105, 292), (105, 372)]),
             (6, [(215, 292), (215, 372)]),
             (7, [(420, 44), (420, -40)])]
    calls += [(8 + i, [(k[f"KNOB {i + 1}"][0], 100 - 27), (k[f"KNOB {i + 1}"][0], -40)]) for i in range(4)]
    # the top row's leaders go up between the knobs (14 and 15 share a gap: 15 goes higher), the bottom row's down
    for i, tx in enumerate([566, 672, 750, 782, 861, 968]):
        cx = gx + gw // 2 + i * dx
        pts = [(cx, gy)] + ([(cx, 150), (tx, 150)] if tx != cx else []) + [(tx, -140 if i == 3 else -95)]
        calls.append((12 + i, pts))
    calls += [(18 + i, [(gx + gw // 2 + i * dx, gy + dy + gh), (gx + gw // 2 + i * dx, 372)]) for i in range(6)]
    return ('<svg class="fm1" viewBox="-70 -170 1085 565" role="img" aria-labelledby="fig-panel">\n'
            + "\n".join(s) + "\n" + _svg_callouts(calls) + "\n</svg>")


def keys_svg():
    """the keys below the face, numbered 24-35"""
    c = FM1_COLOURS
    x0, y0, w, h = FM1_KEYBED
    pitch = (w - 24) / 16
    kw, kh = pitch * 0.78, h * 0.425
    s = [f'<path d="M4,318 V580 A40,40 0 0 0 44,620 H956 A40,40 0 0 0 996,580 V318 Z" fill="{c["body"]}"/>',
         '<line x1="4" y1="318" x2="996" y2="318" class="cut"/>',
         f'<rect x="{x0}" y="{y0}" width="{w}" height="{h}" rx="18" fill="{c["tray"]}"/>']
    key = lambda cx, cy, fill: (f'<rect x="{cx - kw / 2:.1f}" y="{cy - kh / 2:.1f}" width="{kw:.1f}" height="{kh:.1f}" '
                                f'rx="{kw / 2:.1f}" fill="{fill}" stroke="{c["edge"]}" stroke-width="1.5"/>')
    whites = [x0 + 12 + (i + 0.5) * pitch for i in range(16)]
    blacks = [x0 + 12 + (a + 1) * pitch for a in FM1_BLACK_AFTER]
    s += [key(x, y0 + h * 0.75, "#d9d6d0") for x in whites]
    s += [key(x, y0 + h * 0.25, c["part"]) for x in blacks]
    top = y0 + h * 0.25 - kh / 2
    calls = [(24 + j, [(x, top), (x, 282)]) for j, x in enumerate(blacks)]
    # one bracket under all the white keys
    by, l, r = 600, whites[0] - kw / 2, whites[-1] + kw / 2
    s.append(f'<polyline points="{l:.1f},{by - 8} {l:.1f},{by} {r:.1f},{by} {r:.1f},{by - 8}" class="lead"/>')
    calls.append((35, [(500, by), (500, 662)]))
    return ('<svg class="fm1" viewBox="-20 255 1040 430" role="img" aria-labelledby="fig-keys">\n'
            + "\n".join(s) + "\n" + _svg_callouts(calls) + "\n</svg>")

def controls():
    """the section "Controls": the two drawings and the table of what each number does"""
    rows = []
    for nums, name, what in CONTROLS:
        names = name if isinstance(name, list) else [name]
        for i, (n, nm) in enumerate(zip(nums, names)):
            cell = f'<td rowspan="{len(nums)}">{what}</td>' if i == 0 else ""
            rows.append(f'<tr id="ctl-{n}" data-n="{n}"><td class="n"><a href="#fig-{"panel" if n < 24 else "keys"}">{n}</a></td>'
                        f"<td><b>{nm}</b></td>{cell}</tr>")
    pages = "\n".join("<tr>" + "".join(f"<td>{x}</td>" for x in r) + "</tr>" for r in KNOB_PAGES)
    return f"""<h2 id="controls">Controls</h2>
<p>The numbers on the drawings are in the table below them; a number links to its row, and a row's number back to its drawing. The names are those printed on the FM-1. Black keys are counted from the left.</p>
<figure class="fm1-fig" id="fig-panel">
<div class="fm1-scroll">{panel_svg()}</div>
<figcaption>The panel (1–23).<span class="narrow"> Swipe sideways to see all of it.</span></figcaption>
</figure>
<figure class="fm1-fig" id="fig-keys">
<div class="fm1-scroll">{keys_svg()}</div>
<figcaption>The keys (24–35).<span class="narrow"> Swipe sideways to see all of it.</span></figcaption>
</figure>
<table class="controls">
<tr><th>No.</th><th>Control</th><th>Under Bigorna</th></tr>
{chr(10).join(rows)}
</table>
<p>KNOB 1–4:</p>
<table class="knob-pages">
<tr><th>Where</th><th>KNOB 1</th><th>KNOB 2</th><th>KNOB 3</th><th>KNOB 4</th></tr>
{pages}
</table>
<p>Every gesture in full: <a href="#playing-it">Playing it</a>.</p>"""

def manual():
    md = (ROOT / "README.md").read_text()
    part = md[md.index("\n# Manual\n"):md.index("\n# Building from source\n")]
    part = part.replace("\n# Manual\n", "\n", 1)
    toc = [(2, "controls", "Controls")]
    body = controls() + "\n" + to_html(part, toc)
    t = "\n".join(f'<a href="#{s}"' + (' class="sub"' if lv == 3 else "") + f">{html.escape(n)}</a>" for lv, s, n in toc)
    bar = ('<div class="mbar" role="search">\n'
           '<input type="search" id="msearch" placeholder="Search the manual" aria-label="Search the manual" '
           'autocomplete="off" aria-controls="mresults" aria-expanded="false">\n'
           '<div id="mresults" class="mresults" role="listbox" hidden></div>\n'
           '</div>')
    # bottom right, once the contents are scrolled past: back to the contents (a list), back to the top (an arrow)
    icon = lambda d: (f'<svg viewBox="0 0 24 24" width="22" height="22" aria-hidden="true" fill="none" stroke="currentColor" '
                      f'stroke-width="2" stroke-linecap="round" stroke-linejoin="round">{d}</svg>')
    float_nav = ('<nav class="mfloat" aria-label="Page">\n'
                 '<a href="#contents" data-tip="Contents" aria-label="Contents">'
                 + icon('<path d="M9 6h11M9 12h11M9 18h11"/><circle cx="4.5" cy="6" r="1"/><circle cx="4.5" cy="12" r="1"/>'
                        '<circle cx="4.5" cy="18" r="1"/>') + '</a>\n'
                 '<a href="#top" data-tip="Back to top" aria-label="Back to top">'
                 + icon('<path d="M12 19V5M5 12l7-7 7 7"/>') + '</a>\n'
                 '</nav>')
    intro = ('<h1 id="top">Manual</h1>\n' + bar + '\n<p>Everything the FM-1 does under Bigorna. New here? Start with '
             '<a href="#controls">Controls</a> and <a href="#playing-it">Playing it</a>; to install, see '
             '<a href="install.html">Install</a>. Press <kbd>/</kbd> to search.</p>\n'
             f'<nav class="toc" id="contents" aria-label="Contents">\n{t}\n</nav>')
    (DOCS / "manual.html").write_text(page("Bigorna manual", "Manual",
                                           '<div class="manual">\n' + intro + "\n" + body + "\n" + float_nav + "\n</div>",
                                           "The Bigorna manual: every control of the FM-1 under Bigorna.",
                                           f'\n<script src="js/manual.js?v={MANUAL_JS_V}" defer></script>'))


def changelog():
    md = (ROOT / "CHANGELOG.md").read_text()
    body = to_html(md, [])
    (DOCS / "changelog.html").write_text(page("Bigorna changelog", "Changelog", body, "What changed in each Bigorna build."))


def firmware(pkg):
    raw = pkg.read_bytes()
    sys.path.insert(0, str(ROOT / "tools"))
    import fm1_install as fi
    product, _ = fi.load_package(str(pkg), False)
    ver = "BUILD " + re.search(r"^BUILD = (\d+)", (ROOT / "tools" / "build.py").read_text(), re.M)[1]
    d = DOCS / "firmware"
    d.mkdir(exist_ok=True)
    for old in d.glob("*.fwsc"):
        old.unlink()
    name = "bigorna.fwsc"
    shutil.copyfile(pkg, d / name)
    (d / "latest.json").write_text(json.dumps({"file": name, "version": ver, "product": product,
                                               "size": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}, indent=1) + "\n")
    print(f"site: firmware {ver} ({product}, {len(raw)} B)")


if __name__ == "__main__":
    fragments()
    manual()
    changelog()
    firmware(Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "bigorna.fwsc")
    print("site: docs/index.html, install.html, backup.html, manual.html, changelog.html, projects.html, apps.html")
