#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# BIGORNA (as TONITOR's): the website's generated parts, in docs/ (GitHub Pages):
#   docs/index.html, docs/install.html, docs/projects.html   site/*.html in the common frame (the home page's demo
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
           ("changelog.html", "Changelog"), ("projects.html", "Other projects"), (REPO, "GitHub")]
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


def manual():
    md = (ROOT / "README.md").read_text()
    part = md[md.index("\n# Manual\n"):md.index("\n# Building from source\n")]
    part = part.replace("\n# Manual\n", "\n", 1)
    toc = []
    body = to_html(part, toc)
    t = "\n".join(f'<a href="#{s}"' + (' class="sub"' if lv == 3 else "") + f">{html.escape(n)}</a>" for lv, s, n in toc)
    intro = ('<h1>Manual</h1>\n<p>Everything the FM-1 does under Bigorna. New here? Start with '
             '<a href="#the-panel">The panel</a>; to install, see <a href="install.html">Install</a>.</p>\n'
             f'<nav class="toc" aria-label="Contents">\n{t}\n</nav>')
    (DOCS / "manual.html").write_text(page("Bigorna manual", "Manual", intro + "\n" + body,
                                           "The Bigorna manual: every control of the FM-1 under Bigorna."))


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
    print("site: docs/index.html, install.html, backup.html, manual.html, changelog.html, projects.html")
