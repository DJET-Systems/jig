#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Build the jig documentation book: docs/ -> static HTML in site/docs/.
# Hand-rolled on purpose: standard library only, no markdown dependency.
import html
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "site", "docs")

BOOK = ["index", "getting-started", "concepts", "projects", "toolchains", "environments",
        "building", "hermeticity", "comparison", "troubleshooting", "faq"]
REFERENCE = ["cli", "manifest", "env-vars", "errors", "platforms", "cheatsheet", "packaging"]


def md_path(slug):
    base = os.path.join(ROOT, "docs", "user", slug + ".md")
    if os.path.exists(base):
        return base, "user"
    return os.path.join(ROOT, "docs", "reference", slug + ".md"), "reference"


def slugify(t):
    s = re.sub(r"[^\w\s-]", "", t.lower())
    return re.sub(r"[\s_-]+", "-", s).strip("-") or "section"


def esc(s):
    return html.escape(s, quote=False)


def inline(s):
    # escape first, then decorate. protect code spans from further substitutions.
    parts = esc(s).split("`")
    for i in range(1, len(parts), 2):
        parts[i] = "\x00" + html.escape(parts[i], quote=False) + "\x01"
        if i + 1 == len(parts):
            break
    s = "".join(parts)
    # markdown-aware link targets are resolved by the caller before inline() runs,
    # so leave plain URLs and decorative syntax alone.
    s = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", s)
    s = re.sub(r"(?<!\w)\*([^*\n]+)\*(?!\w)", r"<em>\1</em>", s)
    s = s.replace("\x00", "<code>").replace("\x01", "</code>")
    return s


def resolve_link(raw, part):
    raw = raw.strip()
    if raw.startswith(("http://", "https://", "#", "mailto:")):
        return raw
    m = re.match(r"^([^#]+)(#.*)?$", raw)
    if not m:
        return raw
    target, frag = m.group(1), m.group(2) or ""
    if not target.endswith(".md"):
        return "#"
    stem = os.path.splitext(os.path.basename(target))[0]
    slugs = BOOK + REFERENCE
    return stem + ".html" + frag if stem in slugs and (part == "user" or True) else "#"


def render_md(text, part):
    out = []
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        ln = lines[i]
        if not ln.strip():
            i += 1
            continue
        if ln.startswith("```"):
            lang = ln[3:].strip()
            i += 1
            buf = []
            while i < len(lines) and not lines[i].startswith("```"):
                buf.append(lines[i])
                i += 1
            i += 1
            cls = ' class="lang-' + cls_safe(lang) + '"' if lang else ""
            out.append("<pre" + cls + ">" + esc("\n".join(buf)) + "</pre>")
            continue
        if ln.lstrip().startswith("|") and i + 1 < len(lines) and re.match(r"^\s*\|[\s:|-]+\|?\s*$", lines[i + 1]):
            head = [c.strip() for c in ln.strip().strip("|").split("|")]
            i += 2
            rows = []
            while i < len(lines) and lines[i].lstrip().startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            buf = ["<div class='scroll'><table class='tbl'>", "<thead><tr>"]
            buf += ["<th>" + inline(c) + "</th>" for c in head]
            buf.append("</tr></thead><tbody>")
            for row in rows:
                buf.append("<tr>" + "".join("<td>" + inline(c) + "</td>" for c in row) + "</tr>")
            out.append("".join(buf) + "</tbody></table></div>")
            continue
        m = re.match(r"^(#{1,4}) (.*)$", ln)
        if m:
            depth, title = len(m.group(1)), inline(m.group(2).strip())
            ident = " id='" + slugify(m.group(2)) + "'" if depth <= 3 else ""
            out.append("<h%d%s>%s</h%d>" % (depth, ident, title, depth))
            i += 1
            continue
        if ln.startswith(">"):
            buf = []
            while i < len(lines) and lines[i].startswith(">"):
                buf.append(lines[i].lstrip("> ").rstrip())
                i += 1
            out.append("<blockquote>" + inline(" ".join(buf)) + "</blockquote>")
            continue
        if re.match(r"^\s*[-*+] ", ln):
            items = []
            while i < len(lines) and re.match(r"^\s*[-*+] ", lines[i]):
                items.append(inline(re.sub(r"^\s*[-*+] ", "", lines[i])))
                i += 1
            out.append("<ul>" + "".join("<li>" + x + "</li>" for x in items) + "</ul>")
            continue
        if re.match(r"^\s*\d+[.)] ", ln):
            items = []
            while i < len(lines) and re.match(r"^\s*\d+[.)] ", lines[i]):
                items.append(inline(re.sub(r"^\s*\d+[.)] ", "", lines[i])))
                i += 1
            out.append("<ol>" + "".join("<li>" + x + "</li>" for x in items) + "</ol>")
            continue
        if ln.strip() == "---":
            out.append("<hr>")
            i += 1
            continue
        # link syntax needs md-aware resolution; handled here before inline()
        paras_raw = [ln]
        i += 1
        while i < len(lines) and lines[i].strip() and not re.match(r"^(#|```|>|\s*[-*+] |\s*\d+[.)] |\s*\|)", lines[i]) and lines[i].strip() != "---":
            paras_raw.append(lines[i])
            i += 1
        joined = " ".join(x.strip() for x in paras_raw)
        joined = re.sub(r"\[([^\]]+)\]\(([^)]+)\)",
                        lambda m: "<a href='" + esc(resolve_link(m.group(2), part)) + "'>" + m.group(1) + "</a>",
                        joined)
        out.append("<p>" + inline(joined) + "</p>")
    headings = out[:1]  # keep h1
    return "\n".join(out)


def cls_safe(lang):
    return re.sub(r"[^a-z0-9-]", "", lang.lower()) or "plain"


def title_of(text):
    for ln in text.split("\n"):
        m = re.match(r"^#{1,2} (.*)$", ln)
        if m:
            return m.group(1).strip()
    return ""


CSS = """
:root {
  --bg: #0b0b0c; --fg: #f2f2f0; --muted: #9a9a96; --rule: #2a2a2c; --panel: #111113;
  --signal: #FF5A1F; --f: "IBM Plex Sans", "Helvetica Neue", Arial, sans-serif;
  --f-mono: "IBM Plex Mono", ui-monospace, "SF Mono", Menlo, Consolas, monospace;
  color-scheme: dark;
}
@media (prefers-color-scheme: light) {
  :root { --bg: #f5f5f6; --fg: #0b0b0c; --muted: #5d5d62; --rule: #d4d4d8; --panel: #ebebed; --signal: #B93204; color-scheme: light; }
}
*, *::before, *::after { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--fg); font: 400 1rem/1.65 var(--f); }
a { color: inherit; }
code, pre { font-family: var(--f-mono); }
:focus-visible { outline: 2px solid var(--fg); outline-offset: 3px; }
.top { border-bottom: 1px solid var(--rule); }
.top .wrap { max-width: 1120px; margin-inline: auto; display: flex; align-items: baseline; gap: 1.5rem; padding: 1rem 1.25rem; }
.brand { font-weight: 700; font-size: 1.25rem; letter-spacing: -0.03em; text-decoration: none; }
.top .labs { font-family: var(--f-mono); font-size: 0.8125rem; color: var(--muted); display: flex; gap: 1.25rem; margin-left: auto; }
.top a { text-decoration: none; }
.split { max-width: 1120px; margin-inline: auto; display: grid; grid-template-columns: minmax(0, 14rem) minmax(0, 1fr); gap: 2.5rem; padding: 2rem 1.25rem; }
aside { font-family: var(--f-mono); font-size: 0.8125rem; position: sticky; align-self: start; top: 1rem; }
aside .part { color: var(--muted); letter-spacing: 0.08em; text-transform: uppercase; font-size: 0.7rem; margin: 1.5rem 0 0.5rem; }
aside .part:first-child { margin-top: 0; }
aside ol { list-style: none; margin: 0; padding: 0; display: grid; gap: 0.3rem; }
aside a { text-decoration: none; color: var(--muted); display: grid; grid-template-columns: 1.6rem minmax(0, 1fr); }
aside a:hover { color: var(--fg); }
aside a.on { color: var(--fg); }
aside .n { color: var(--muted); }
.page { max-width: 46rem; }
.page > .label { font-family: var(--f-mono); font-size: 0.8125rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--muted); margin: 0; }
.page h1 { font-size: clamp(1.9rem, 4vw, 2.6rem); line-height: 1.1; letter-spacing: -0.02em; margin: 0.5rem 0 2rem; }
.page h2 { font-size: 1.5rem; line-height: 1.2; letter-spacing: -0.01em; margin: 2.5rem 0 0.75rem; }
.page h3 { font-size: 1.125rem; margin: 2rem 0 0.5rem; }
.page p { margin: 0 0 1rem; }
.page ul, .page ol { margin: 0 0 1rem; padding-left: 1.4rem; }
.page li { margin-block: 0.3rem; }
.page blockquote { margin: 0 0 1rem; padding: 0.75rem 1rem; border-left: 2px solid var(--signal); color: var(--muted); }
.page pre { margin: 0 0 1rem; padding: 1rem 1.1rem; border: 1px solid var(--rule); background: var(--panel); overflow-x: auto; font-size: 0.8125rem; line-height: 1.65; }
.page code { font-size: 0.9em; }
.page p code, .page li code, .page h2 code, .page h3 code { border: 1px solid var(--rule); padding: 0.05em 0.35em; }
.page table { width: 100%; border-collapse: collapse; margin: 0 0 1rem; font-size: 0.875rem; }
.page th, .page td { text-align: left; padding: 0.55rem 0.75rem 0.55rem 0; border-bottom: 1px solid var(--rule); vertical-align: top; }
.page th { font-family: var(--f-mono); font-size: 0.75rem; letter-spacing: 0.06em; text-transform: uppercase; color: var(--muted); font-weight: 500; }
.page hr { border: 0; border-top: 1px solid var(--rule); margin: 2rem 0; }
.scroll { overflow-x: auto; }
.pager { display: flex; justify-content: space-between; gap: 1rem; margin-top: 3rem; border-top: 1px solid var(--rule); padding-top: 1.25rem; font-family: var(--f-mono); font-size: 0.8125rem; }
.pager a { text-decoration: none; color: var(--muted); }
.pager a:hover { color: var(--fg); }
.pager b { display: block; color: var(--fg); font-weight: 500; margin-top: 0.2rem; }
.pager .next { text-align: right; }
@media (max-width: 900px) {
  .split { grid-template-columns: minmax(0, 1fr); gap: 1.5rem; }
  aside { position: static; }
  aside ol { display: none; }
}
"""

TEMPLATE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title} · jig docs</title>
<meta name="description" content="{desc}">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans:wght@400;500;600;700&display=swap">
<style>{css}</style>
</head>
<body>
<header class="top">
  <div class="wrap">
    <a class="brand" href="../index.html">jig</a>
    <nav class="labs" aria-label="Site">
      <a href="https://github.com/ansgrb/jig">GitHub ↗</a>
    </nav>
  </div>
</header>
<div class="split">
  <aside aria-label="Chapters">
    <p class="part">Book</p>
    <ol>{book_nav}</ol>
    <p class="part">Reference</p>
    <ol>{ref_nav}</ol>
  </aside>
  <main class="page">{body}{pager}</main>
</div>
</body>
</html>
"""


def nav_html(chapters, current):
    items = []
    for n, slug in enumerate(chapters, 1):
        on = slug == current
        cls = " class='on'" if on else ""
        label = esc(CHAPTER_TITLES[slug])
        items.append("<li><a%s href='%s'><span class='n'>%02d</span><span>%s</span></a></li>"
                     % (cls, slug + ".html", n, label))
    return "".join(items)


def pager_html(chapters, current):
    idx = chapters.index(current)
    parts = []
    if idx > 0:
        prev = chapters[idx - 1]
        parts.append("<a href='%s'><span>Previous</span><b>%s</b></a>" % (prev + ".html", esc(CHAPTER_TITLES[prev])))
    else:
        parts.append("<span></span>")
    if idx < len(chapters) - 1:
        nxt = chapters[idx + 1]
        parts.append("<a class='next' href='%s'><span>Next</span><b>%s</b></a>" % (nxt + ".html", esc(CHAPTER_TITLES[nxt])))
    return "<footer class='pager'>" + "".join(parts) + "</footer>"


CHAPTER_TITLES = {}
SOURCES = {}


def main():
    os.makedirs(OUT, exist_ok=True)
    for slug in BOOK + REFERENCE:
        path, part = md_path(slug)
        raw = open(path, encoding="utf-8").read()
        CHAPTER_TITLES[slug] = title_of(raw)
        SOURCES[slug] = (render_md(raw, part), part)
    pages = BOOK + REFERENCE
    for slug in pages:
        body, part = SOURCES[slug]
        part_label = "Book" if slug in BOOK else "Reference"
        number = (BOOK if part_label == "Book" else REFERENCE).index(slug) + 1
        pager = pager_html(BOOK if part_label == "Book" else REFERENCE, slug)
        doc = TEMPLATE.format(
            title=esc(CHAPTER_TITLES[slug]),
            desc=esc(CHAPTER_TITLES[slug]) + " · jig, the native C/C++ toolchain.",
            css=CSS,
            book_nav=nav_html(BOOK, slug),
            ref_nav=nav_html(REFERENCE, slug),
            body="<p class='label'>%02d · %s</p>" % (number, part_label) + body,
            pager=pager,
        )
        with open(os.path.join(OUT, slug + ".html"), "w", encoding="utf-8") as f:
            f.write(doc)
        print("wrote", os.path.relpath(os.path.join(OUT, slug + ".html"), ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
