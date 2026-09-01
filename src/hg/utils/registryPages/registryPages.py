#!/usr/bin/env python3
"""registryPages.py - draw two web pages from the four configuration catalogs.

Refs #37838, #37923, #37925, #37623.  The four catalogs each answer for one
part of the browser's configuration surface, and each one prints its own page.
This draws the two pages that need all four at once:

  registryVenn.html    a four-set Venn of the registries, so the shape of the
                       whole surface is visible at once: which registry owns
                       what, and the handful of names more than one describes.

  registryIndex.html   every name in all four catalogs, readable either in each
                       catalog's own groups or as one alphabetical list, with
                       the description on hover.

Everything on both pages is computed from the catalogs.  No count is typed in
here, so the pages cannot drift from the tree the way a hand-written summary
would.  registryData.py does the reading and the matching; this file does the
drawing.

Usage:
    registryPages.py --outDir ~/public_html          # both pages
    registryPages.py --venn v.html --index i.html    # or name them
    registryPages.py --check                         # no output, just the audit
    registryPages.py --outDir DIR --audit            # add the saved-session count

--check is the mode for a nightly cron.  It writes nothing and fails when a name
starts or stops being shared between two registries, which is the one thing here
that needs a person to read a call site.  See KNOWN_SHARED in registryData.py.

--audit runs sessionCartAudit, which needs the database and takes about fifteen
seconds.  Without it the pages leave out the one paragraph that talks about real
saved sessions.
"""

import argparse
import datetime
import html
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import registryData as rd                                          # noqa: E402
import trackDbData as td                                           # noqa: E402


NUM_WORD = {0: "no", 1: "one", 2: "two", 3: "three", 4: "four", 5: "five", 6: "six",
            7: "seven", 8: "eight", 9: "nine", 10: "ten", 11: "eleven", 12: "twelve",
            13: "thirteen", 14: "fourteen", 15: "fifteen", 16: "sixteen",
            17: "seventeen", 18: "eighteen", 19: "nineteen", 20: "twenty"}


def word(n):
    """Small numbers read better spelled out in a sentence."""
    return NUM_WORD.get(n, "{:,}".format(n))


def esc(s):
    return html.escape(s, quote=False)


def shortPath(path):
    """Write a path under the user's home as ~/... so provenance is readable."""
    home = os.path.expanduser("~")
    return "~" + path[len(home):] if path.startswith(home + os.sep) else path


def asset(name):
    """Read one of the stylesheet or script files that sits next to this one."""
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), name)) as f:
        return f.read().rstrip("\n")


def style(name):
    """The shared tokens plus one page's own rules, as a single stylesheet."""
    return asset("tokens.css") + "\n\n" + asset(name)


# ============================================================ the Venn page ==

# Four congruent ellipses in the classic four-set arrangement: two rotated one
# way, two the other, so all fifteen regions exist.  Order matters, and it is
# the order in rd.REG_ORDER: the two outer ellipses are the first and last.
ELLIPSES = {
    "track": (350, 418, 360, 225, -140),
    "url":   (450, 318, 360, 225, -140),
    "file":  (544, 318, 360, 225,  -40),
    "conf":  (644, 418, 360, 225,  -40),
}

VIEWBOX = (1000, 730)

# Where each region's label goes.  Found by rasterizing the four ellipses and
# taking the deepest point of each region, then checked so that every corner of
# every text block lands inside the region it belongs to.  Do not nudge these by
# eye: change the ellipses and these have to be found again.
#   big    the exclusive region of one registry: rule, count, name, ticket
#   pair   two registries: the count with a colored dot per registry, or a zero
#   small  three or four registries: a zero, since none of them has ever held a name
REGION_LABEL = {
    ("track",): ("big", 150, 417),
    ("url",):   ("big", 338,  80),
    ("file",):  ("big", 655,  80),
    ("conf",):  ("big", 843, 417),

    ("track", "url"):  ("pair", 257, 166),
    ("url", "file"):   ("pair", 495, 166),
    ("track", "file"): ("pair", 290, 511),
    ("url", "conf"):   ("pair", 703, 511),
    ("file", "conf"):  ("pair", 737, 166),
    ("track", "conf"): ("pair", 497, 614),

    ("track", "url", "file"):  ("small", 400, 252),
    ("url", "file", "conf"):   ("small", 593, 252),
    ("track", "file", "conf"): ("small", 401, 588),
    ("track", "url", "conf"):  ("small", 593, 588),

    ("track", "url", "file", "conf"): ("small", 497, 514),
}

REG_SHORT = {"track": "CART / TRACK", "url": "URL PARAMS",
             "file": "CART / FILE", "conf": "HG.CONF"}

REG_PHRASE = {"track": "track cart variables", "url": "URL parameters",
              "file": "file cart variables", "conf": "hg.conf settings"}

# The same four names at the head of a sentence or a table cell.  A plain
# .capitalize() would turn "URL parameters" into "Url parameters".
REG_ONLY = {"track": "Track cart variables only", "url": "URL parameters only",
            "file": "File cart variables only", "conf": "hg.conf settings only"}

REG_BRIEF = {"track": "Track cart", "url": "URL", "file": "File cart", "conf": "hg.conf"}


def vennSvg(regs, counts):
    """The four-set diagram, with the count of every region drawn in it."""
    byKey = {r["key"]: r for r in regs}
    out = ['<svg class="venn" viewBox="0 0 %d %d" role="img"' % VIEWBOX,
           '     aria-label="%s">' % esc(vennAria(regs, counts))]

    out.append('  <g fill-opacity="0.11" stroke-width="2.25" '
               'style="fill-opacity:var(--fill)">')
    for key in rd.REG_ORDER:
        cx, cy, rx, ry, rot = ELLIPSES[key]
        reg = byKey[key]
        out.append('    <ellipse cx="%d" cy="%d" rx="%d" ry="%d" '
                   'transform="rotate(%d %d %d)"' % (cx, cy, rx, ry, rot, cx, cy))
        out.append('             fill="var(--s-%s)" stroke="var(--s-%s)">' % (key, key))
        out.append('      <title>%s (#%s): %d rows, %d distinct names</title></ellipse>'
                   % (esc(reg["title"]), reg["ticket"], reg["rows"], len(reg["names"])))
    out.append('  </g>')

    out.append('  <g text-anchor="middle">')
    for region in sorted(REGION_LABEL, key=lambda r: (len(r), r)):
        kind, x, y = REGION_LABEL[region]
        n = counts.get(region, 0)
        if kind == "big":
            key = region[0]
            out.append('    <rect x="%d" y="%d" width="60" height="3" fill="var(--s-%s)"/>'
                       % (x - 30, y - 39, key))
            out.append('    <text class="r-count" x="%d" y="%d" font-size="32">%d</text>'
                       % (x, y, n))
            out.append('    <text class="r-name" x="%d" y="%d">%s</text>'
                       % (x, y + 20, REG_SHORT[key]))
            out.append('    <text class="tick" x="%d" y="%d">#%s</text>'
                       % (x, y + 38, byKey[key]["ticket"]))
        elif kind == "pair" and n:
            out.append('    <text class="r-small" x="%d" y="%d">%d</text>' % (x, y, n))
            for i, key in enumerate(region):
                out.append('    <circle cx="%d" cy="%d" r="4.5" fill="var(--s-%s)"/>'
                           % (x - 7 + 14 * i, y + 14, key))
        else:
            out.append('    <text class="r-zero" x="%d" y="%d">%d</text>' % (x, y + 6, n))
    out.append('  </g>')
    out.append('</svg>')
    return "\n".join(out)


def vennAria(regs, counts):
    """One sentence saying what the picture shows, for a reader who cannot see it."""
    only = ", ".join(str(counts.get((r["key"],), 0)) for r in regs)
    pairs = sorted((n, r) for r, n in counts.items() if len(r) == 2 and n)
    pairText = ", ".join(str(n) for n, _ in pairs)
    names = [REG_PHRASE[r["key"]] for r in regs]
    listed = ", ".join(names[:-1]) + " and " + names[-1]
    return ("A four-ellipse Venn diagram of the browser's configuration registries: %s. "
            "The exclusive regions hold %s names. %s pairwise regions hold %s. The other "
            "regions, including every three-way and four-way region, are empty."
            % (listed, only, word(len(pairs)).capitalize(), pairText))


def sliverList(regs, shared):
    """The shared names, one block per pair of registries, with the catalogs' own notes."""
    byKey = {r["key"]: r for r in regs}
    note = {}
    for reg in regs:
        for group in reg["groups"]:
            for r in group["rows"]:
                if r["desc"]:
                    note.setdefault((reg["key"], r["name"]), r["desc"])

    pairs = {}
    for name, keys in shared.items():
        pairs.setdefault(keys, []).append(name)

    out = ['<div class="slivers">']
    for keys in sorted(pairs, key=lambda k: (-len(pairs[k]), k)):
        names = sorted(pairs[keys], key=lambda n: n.lower())
        out.append('  <div class="sliver">')
        out.append('    <div class="sliver-head">')
        out.append('      <div class="pairchips">%s'
                   % "".join('<span class="chip %s"></span>' % k for k in keys))
        out.append('        <span class="pairname">%s</span></div>'
                   % esc(" + ".join(REG_PHRASE[k] for k in keys)))
        out.append('      <div class="pairnum">%d<span>%s</span></div>'
                   % (len(names), "name" if len(names) == 1 else "names"))
        out.append('    </div>')
        out.append('    <ul class="names">')
        for name in names:
            parts = []
            for key in keys:
                text = note.get((key, name))
                if text:
                    parts.append('<b>%s:</b> %s' % (esc(byKey[key]["title"]), esc(text)))
            out.append('      <li><code>%s</code><span class="note">%s</span></li>'
                       % (esc(name), " ".join(parts) or "&mdash;"))
        out.append('    </ul>')
        out.append('  </div>')
    out.append('</div>')
    return "\n".join(out)


def regionTable(regs, counts):
    """The same figure as a table, so identity is never carried by color alone."""
    total = sum(counts.values())
    rows = []
    order = sorted(REGION_LABEL, key=lambda r: (len(r), -counts.get(r, 0), r))
    for region in order:
        n = counts.get(region, 0)
        dots = "".join('<span class="dot %s"></span>' % (k if k in region else "off")
                       for k in rd.REG_ORDER)
        if len(region) == 1:
            label = REG_ONLY[region[0]]
        elif len(region) == len(rd.REG_ORDER):
            label = "All four"
        else:
            label = " + ".join(REG_BRIEF[k] for k in region)
        rows.append('        <tr%s><td><span class="dots">%s</span></td><td>%s</td>'
                    '<td class="num">%d</td></tr>'
                    % ('' if n else ' class="z"', dots, esc(label), n))
    return ('    <table>\n'
            '      <caption>Region counts, %s distinct names</caption>\n'
            '      <thead>\n'
            '        <tr><th scope="col">Registries</th><th scope="col">Region</th>'
            '<th scope="col" style="text-align:right">Names</th></tr>\n'
            '      </thead>\n'
            '      <tbody>\n%s\n      </tbody>\n'
            '    </table>' % ("{:,}".format(total), "\n".join(rows)))


def collisionNote(coll):
    """The names that match across registries without being the same variable."""
    if not coll:
        return ("<p>No name is spelled the same way in two registries while meaning two "
                "different variables.</p>")
    lines = []
    for name, where in coll:
        keys = [k for k in rd.REG_ORDER if k in where]
        lines.append("<code>%s</code> is %s" % (esc(name), " and ".join(
            "%s in the %s" % (", ".join("<code>%s</code>" % esc(v) for v in where[k]),
                              esc(REG_PHRASE[k])) for k in keys)))
    return ("<p>%s %s in two registries and mean two different variables. They are "
            "counted as separate names above.</p>\n      <p>%s.</p>"
            % (word(len(coll)).capitalize(),
               "spelling appears" if len(coll) == 1 else "spellings appear",
               ". ".join(lines)))


def vennPage(regs, counts, shared, coll, baseline, audit, indexName, corrName, today):
    """The whole Venn page."""
    byKey = {r["key"]: r for r in regs}
    total = sum(counts.values())
    rowTotal = sum(r["rows"] for r in regs)
    alone = sum(n for r, n in counts.items() if len(r) == 1)
    nShared = total - alone

    track = byKey["track"]
    prefixes = next(g for g in track["groups"] if g["title"].endswith("the track name"))
    exceptions = next((g for g in track["groups"]
                       if g["title"].startswith("Exceptions")), {"rows": []})
    plain = track["rows"] - len(prefixes["rows"]) - len(exceptions["rows"])

    confShared = sorted(n for n, keys in shared.items() if "conf" in keys)
    if confShared == ["textSize"]:
        confExcept = ("<code>textSize</code> is the exception, and it is deliberate. The setting "
                      "names the site default and the cart holds the visitor's choice.")
    elif confShared:
        confExcept = ("The %s a visitor can also set: %s."
                      % ("one" if len(confShared) == 1 else "ones",
                         ", ".join("<code>%s</code>" % esc(n) for n in confShared)))
    else:
        confExcept = "Nothing a mirror admin sets in hg.conf can be set by a visitor."

    auditPara = ""
    if audit:
        auditPara = ("\n      <p>The session audit reads {sessions:,} saved sessions and finds "
                     "{names:,} distinct variable names in them. {unknown:,} match nothing in "
                     "any catalog.</p>".format(**audit))

    return """<title>Four Config Registries</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wght@500;600;700\
&family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Serif:ital,wght@0,400;0,600;1,400&display=swap">
<style>
%(css)s
</style>

<div class="wrap">

<header>
  <p class="eyebrow">UCSC Genome Browser &middot; configuration surface</p>
  <h1>Four registries, %(sharedWord)s shared names</h1>
  <p class="lede">Four catalogs in <code>hg/utils/</code> describe what can be configured in the
  browser: the settings in <strong>hg.conf</strong>, the parameters on a <strong>CGI URL</strong>,
  the <strong>track-scoped cart variables</strong>, and the <strong>cart variables that hold a
  file name</strong>. Together they hold %(rowTotal)s rows covering %(total)s distinct names. Only
  %(sharedWord)s names appear in more than one registry, and no name appears in three.</p>
  <p class="lede" style="margin-top:12px">A name is in a registry when that catalog gives it a
  row. For the track catalog that means its %(plain)d variables, its %(prefixes)d track-name
  prefixes, and the %(exceptions)d names spelled out in its exceptions list. Every name is listed,
  with its description, in the <a href="%(indexName)s">Registry Name Index</a>.</p>
  <p class="meta">
    <span>generated %(today)s</span>
    <span>source: each catalog's <code style="background:none;padding:0">--json</code></span>
    <span>tree: %(tree)s</span>
  </p>
</header>

<figure>
  <div class="svg-scroll">
%(svg)s
  </div>
  <figcaption>Every name in the four registries, placed by which registries hold it. A pair of
  dots marks which two registries share a region. The four ellipses are nearly disjoint:
  %(alone)s of the %(total)s names sit alone in one registry, %(sharedWord)s sit in a pair, and
  every three-way and four-way region is empty.</figcaption>
</figure>

<section>
  <h2>The %(sharedWord)s shared names</h2>
  <p class="sub">Each of these is one variable that two registries both describe, not two
  variables that happen to share a spelling. The text is each catalog's own.</p>

%(slivers)s
</section>

<section>
  <h2>All %(nRegions)s regions</h2>
  <p class="sub">The same figure as a table. Filled dots mark the registries that hold the names
  in that region.</p>
  <div class="tablewrap">
%(table)s
  </div>
</section>

<section>
  <h2>Reading the empty regions</h2>
  <p class="sub">%(emptyWord)s of the %(nRegions)s regions are empty, and the %(fullWord)s that
  are not hold %(sharedWord)s names between them. Three things explain that, and one of them is
  a gap.</p>
  <div class="notes">

    <div class="notecard">
      <h3>Some names collide but are not shared</h3>
      %(collisions)s
      <p>A bare name is not an identity. Matching on the name alone would have missed the
      track-scoped names, which one catalog spells <code>&lt;track&gt;_sel</code> and the other
      spells <code>_sel</code>, and would have claimed those collisions instead.</p>
    </div>

    <div class="notecard">
      <h3>hg.conf barely touches the rest</h3>
      <p>%(confNames)d settings, %(confShared)s of which a visitor can also set. That is the
      shape you want: what a mirror admin configures and what a visitor configures are two
      different sets.</p>
      <p>%(confExcept)s</p>
    </div>

    <div class="notecard">
      <h3>What sits outside all four</h3>
      <p>%(urlBaseline)d URL names and %(trackBaseline)d cart variable names are recorded in the
      baseline files as out of scope. Those files were accepted wholesale on the day they were
      written, so a name being in one is not evidence that anybody reviewed it.</p>
      <p>Global cart variables, the ones scoped to no track, have no registry at all.
      <code>textSize</code> is one of them, which is why it enters the picture through the URL
      registry rather than a cart one.</p>%(auditPara)s
    </div>

  </div>
</section>

<footer>
  Registries: %(footRegs)s<br>
  Counts taken %(today)s from each catalog's --json output. A track variable is matched on the
  cart variable it names, not on the suffix the catalog stores.<br>
  Name by name, with descriptions: <a href="%(indexName)s">Registry Name Index</a> &middot;
  which of these a trackDb setting can preset: <a href="%(corrName)s">trackDb Override Map</a><br>
  Drawn by hg/utils/registryPages/registryPages.py.
</footer>

</div>
""" % {
        "css": style("venn.css"),
        "today": today,
        "tree": esc(shortPath(rd.kentSrc())),
        "svg": "\n".join("  " + line for line in vennSvg(regs, counts).splitlines()),
        "rowTotal": "{:,}".format(rowTotal),
        "total": "{:,}".format(total),
        "alone": "{:,}".format(alone),
        "sharedWord": word(nShared),
        "plain": plain,
        "prefixes": len(prefixes["rows"]),
        "exceptions": len(exceptions["rows"]),
        "slivers": sliverList(regs, shared),
        "table": regionTable(regs, counts),
        "nRegions": word(len(REGION_LABEL)),
        "emptyWord": word(sum(1 for r in REGION_LABEL if not counts.get(r, 0))).capitalize(),
        "fullWord": word(sum(1 for r in REGION_LABEL
                             if counts.get(r, 0) and len(r) > 1)),
        "collisions": collisionNote(coll),
        "confNames": len(byKey["conf"]["names"]),
        "confShared": word(len(confShared)),
        "confExcept": confExcept,
        "urlBaseline": baseline["url"],
        "trackBaseline": baseline["track"],
        "auditPara": auditPara,
        "indexName": esc(indexName),
        "corrName": esc(corrName),
        "footRegs": " &middot; ".join("%s #%s" % (r["tool"], r["ticket"]) for r in regs),
    }


# =========================================================== the index page ==

def sortKey(name):
    """Sort a name by the part that varies, setting the shared <track> scope aside.

    Every track-scoped name starts with the same seven characters, so sorting on
    the raw string files two hundred and seventy-odd names under punctuation and
    leaves the alphabet half empty.  Dropping the scope puts <track>.heightPer
    under H, where somebody looking for heightPer will go.
    """
    key = name
    for prefix in ("<track>.", "<track>_", "<track>"):
        if key.startswith(prefix):
            key = key[len(prefix):]
            break
    key = key.lstrip("_.<")
    return (key.lower() or name.lower(), name.lower())


def bucketOf(name):
    """The letter a name is filed under, or # when it starts with punctuation."""
    first = sortKey(name)[0][:1]
    return first.upper() if first.isalpha() else "#"


def groupedView(regs, shared, tips):
    """One section per registry, in the catalog's own groups."""
    out, nav = [], []
    for reg in regs:
        key = reg["key"]
        nav.append('<a class="navlink %s" href="#%s"><i class="sw %s"></i>%s<em>%d</em></a>'
                   % (key, key, key, esc(reg["title"]), reg["rows"]))
        body = []
        for group in reg["groups"]:
            chips = []
            for r in group["rows"]:
                i = len(tips)
                tips.append([r["name"], r["desc"], r["meta"]])
                also = [k for k in shared.get(r["name"], ()) if k != key]
                dots = "".join('<i class="sh %s" aria-hidden="true"></i>' % k for k in also)
                chips.append('<span class="nm%s" tabindex="0" data-i="%d" data-n="%s">%s%s</span>'
                             % (" also-in" if also else "", i,
                                html.escape(r["name"].lower(), quote=True),
                                html.escape(r["name"]), dots))
            what = ('<p class="gw">%s</p>' % esc(group["what"])) if group["what"] else ""
            body.append('<div class="grp"><div class="ghead"><h3>%s</h3>'
                        '<span class="gn">%d</span></div>%s<div class="chips">%s</div></div>'
                        % (esc(group["title"]), len(group["rows"]), what, "".join(chips)))
        out.append('<section class="reg %s" id="%s">\n'
                   '  <div class="rhead">\n'
                   '    <p class="reyebrow"><i class="sw %s"></i>%s &middot; Redmine #%s</p>\n'
                   '    <h2>%s</h2>\n'
                   '    <p class="rblurb">%s</p>\n'
                   '    <p class="rstat"><strong>%d</strong> rows &middot; <strong>%d</strong> '
                   'distinct names &middot; <strong>%d</strong> groups</p>\n'
                   '  </div>\n  %s\n</section>'
                   % (key, key, key, esc(reg["tool"]), reg["ticket"], esc(reg["title"]),
                      esc(reg["blurb"]), reg["rows"], len(reg["names"]), len(reg["groups"]),
                      "\n  ".join(body)))
    return "\n".join(out), "".join(nav)


def alphabeticalView(regs):
    """All four registries merged into one A to Z list.

    A name two registries hold appears once, and its entry carries a block per
    registry.  A name with several rows in one registry, because several track
    types set their own default for it, keeps every row.
    """
    merged = {}
    for reg in regs:
        for group in reg["groups"]:
            for r in group["rows"]:
                merged.setdefault(r["name"], {}).setdefault(reg["key"], []).append(
                    [r["desc"], r["meta"]])

    names = sorted(merged, key=sortKey)
    data = [[n, [[k, merged[n][k]] for k in rd.REG_ORDER if k in merged[n]]] for n in names]

    buckets = {}
    for i, name in enumerate(names):
        buckets.setdefault(bucketOf(name), []).append(i)
    letters = sorted(b for b in buckets if b != "#")
    if "#" in buckets:
        letters.append("#")

    jump, parts = [], []
    for letter in letters:
        anchor = "az-" + ("sym" if letter == "#" else letter)
        jump.append('<a class="jl" href="#%s">%s</a>' % (anchor, letter))
        chips = []
        for i in buckets[letter]:
            name = names[i]
            dots = "".join('<i class="sh %s" aria-hidden="true"></i>' % k
                           for k in rd.REG_ORDER if k in merged[name])
            chips.append('<span class="nm" tabindex="0" data-j="%d" data-n="%s">%s%s</span>'
                         % (i, html.escape(name.lower(), quote=True),
                            html.escape(name), dots))
        parts.append('<div class="grp azgrp"><div class="ghead"><h3 id="%s">%s</h3>'
                     '<span class="gn">%d</span></div><div class="chips">%s</div></div>'
                     % (anchor, letter, len(buckets[letter]), "".join(chips)))
    markup = '<nav class="jump">%s</nav>\n%s' % ("".join(jump), "\n".join(parts))
    scoped = sum(1 for n in names if n.startswith("<track>") and n != "<track>")
    return markup, data, len(buckets.get("#", [])), scoped


def indexPage(regs, shared, vennName, corrName, today):
    """The whole name index, both views."""
    import json
    tips = []
    grouped, nav = groupedView(regs, shared, tips)
    azMarkup, azData, nSymbols, nScoped = alphabeticalView(regs)

    rowTotal = sum(r["rows"] for r in regs)
    nGroups = sum(len(r["groups"]) for r in regs)
    nNames = len(azData)

    return """<title>Registry Name Index</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wght@500;600;700\
&family=IBM+Plex+Mono:wght@400;500;600&family=IBM+Plex+Serif:ital,wght@0,400;0,600;1,400\
&display=swap">
<style>
%(css)s
</style>

<div class="wrap">

<header>
  <p class="eyebrow">UCSC Genome Browser &middot; configuration surface</p>
  <h1>Registry Name Index</h1>
  <p class="lede">Every name in the four configuration catalogs. Hover a name, or tab to it, to
  read its description and where the tree reads it. <strong>%(nNames)s</strong> distinct names,
  held in <strong>%(rowTotal)s</strong> catalog rows.</p>
  <p class="lede" style="margin-top:10px">Read it two ways. <strong>By group</strong> keeps each
  catalog's own arrangement, one section per registry. <strong>A to Z</strong> merges all four
  into one alphabetical list, where a name that two registries hold appears once and its tooltip
  carries both descriptions.</p>
  <div class="legend">
%(legend)s
    <span><i class="sh" style="background:var(--ink3)"></i>in A to Z, a dot for every registry
    that holds the name; by group, a dot when a second registry holds the same variable</span>
  </div>
</header>

<div class="controls">
  <div class="searchrow">
    <div class="views" role="group" aria-label="View">
      <button type="button" id="vGroup" aria-pressed="true">By group</button>
      <button type="button" id="vAz" aria-pressed="false">A to Z</button>
    </div>
    <input type="search" id="q" placeholder="filter by name, for example  hgt.  or  filter  or\
  login" autocomplete="off" spellcheck="false">
    <span class="count" id="cnt"></span>
  </div>
  <nav class="nav" id="regnav">%(nav)s</nav>
</div>

<p class="empty" id="empty">No name matches that.</p>

<div id="grouped">
%(grouped)s
</div>

<section id="az" aria-label="All names in alphabetical order">
  <div class="rhead" style="border-top-color:var(--ink3)">
    <p class="reyebrow">all four registries &middot; one list</p>
    <h2>A to Z</h2>
    <p class="rblurb">The %(nNames)s distinct names from all four catalogs, merged and sorted.
    Sorting sets aside the <code class="il">&lt;track&gt;</code> scope that every track-scoped
    name shares, so <code class="il">&lt;track&gt;.heightPer</code> is filed under H rather than
    under the punctuation with the other %(nScoped)d names that carry that scope. The
    <code class="il">#</code> bucket at the end holds the %(nSymbols)s names that start with
    punctuation.</p>
  </div>
%(az)s
</section>

<footer>
  Catalogs: %(footRegs)s<br>
  Built %(today)s from each catalog's --json output against %(tree)s. Every description and
  source citation is the catalog's own text, unedited. Where a row carries no prose, the tooltip
  shows what the catalog does record: kind, default, and the file the tree reads it in.<br>
  How the four registries overlap: <a href="%(vennName)s">Four Config Registries</a> &middot;
  which of these a trackDb setting can preset: <a href="%(corrName)s">trackDb Override Map</a><br>
  Drawn by hg/utils/registryPages/registryPages.py.
</footer>

</div>

<div id="tip" role="tooltip" aria-hidden="true"></div>

<script id="tipdata" type="application/json">%(tips)s</script>
<script id="azdata" type="application/json">%(azdata)s</script>
<script>
%(js)s
</script>
""" % {
        "css": style("index.css"),
        "js": asset("index.js"),
        "today": today,
        "tree": esc(shortPath(rd.kentSrc())),
        "nNames": "{:,}".format(nNames),
        "rowTotal": "{:,}".format(rowTotal),
        "nGroups": nGroups,
        "nScoped": nScoped,
        "nSymbols": word(nSymbols),
        "legend": "\n".join('    <span><i class="sw %s"></i>%s</span>'
                            % (r["key"], esc(REG_PHRASE[r["key"]])) for r in regs),
        "nav": nav,
        "grouped": grouped,
        "az": azMarkup,
        "tips": json.dumps(tips, separators=(",", ":")),
        "azdata": json.dumps(azData, separators=(",", ":")),
        "vennName": esc(vennName),
        "corrName": esc(corrName),
        "footRegs": " &middot; ".join("%s #%s" % (r["tool"], r["ticket"]) for r in regs),
    }


# ======================================================= the correlation page ==

def workList(pairs, how, weak=False):
    """The candidate edits, one block per type the docs do not list."""
    groups = td.byMissingType(pairs, how)
    if not groups:
        return '<p class="sub">Nothing. The two files agree on every comparable pair.</p>'
    out = ['<div class="work%s">' % (" weak" if weak else "")]
    for tdbType, ps in groups.items():
        bySetting = {}
        for p in ps:
            bySetting.setdefault(p["setting"], p)
        out.append('  <div class="wrow">')
        out.append('    <div class="wtype"><code>%s</code>'
                   '<span class="wcount">%d %s</span></div>'
                   % (esc(tdbType), len(bySetting),
                      "setting" if len(bySetting) == 1 else "settings"))
        out.append('    <div class="wsettings">')
        for name in sorted(bySetting, key=str.lower):
            p = bySetting[name]
            out.append('      <div class="wsetting"><code>%s</code>'
                       '<span class="wnow">now: %s%s</span></div>'
                       % (esc(name), esc(", ".join(p["docTypes"])),
                          "" if p["how"] == "same name"
                          else "  &middot; joined through %s" % esc(p["var"])))
        out.append('    </div>')
        out.append('  </div>')
    out.append('</div>')
    return "\n".join(out)


def pairTable(pairs):
    """Every setting that has a runtime override, with both type lists."""
    rows = []
    for p in pairs:
        if not p["comparable"]:
            verdict = '<span class="pill none">not comparable</span>'
        elif p["missing"]:
            verdict = '<span class="pill gap">docs may be short</span>'
        elif p["extra"]:
            verdict = '<span class="pill none">cart has no UI for some</span>'
        else:
            verdict = '<span class="pill same">agree</span>'
        cartTypes = " ".join(
            ('<span class="add">%s</span>' % esc(t)) if t in p["missing"] else esc(t)
            for t in p["cartTypes"]) or "&mdash;"
        rows.append(
            '        <tr><td class="mono">%s</td><td class="mono">%s</td>'
            '<td class="how">%s</td><td class="types">%s</td><td class="types">%s</td>'
            '<td>%s</td></tr>'
            % (esc(p["setting"]), esc(p["var"]), esc(p["how"]),
               esc(" ".join(p["docTypes"])) or "&mdash;", cartTypes, verdict))
    return ('    <table>\n'
            '      <caption>%d settings with a runtime override</caption>\n'
            '      <thead>\n'
            '        <tr><th scope="col">trackDb setting</th><th scope="col">cart variable</th>'
            '<th scope="col">joined by</th><th scope="col">docs say</th>'
            '<th scope="col">cart serves</th><th scope="col">verdict</th></tr>\n'
            '      </thead>\n      <tbody>\n%s\n      </tbody>\n    </table>'
            % (len(pairs), "\n".join(rows)))


def chipList(names):
    return '<div class="chips">%s</div>' % "".join(
        '<span class="nm">%s</span>' % html.escape(n) for n in names)


def correlatePage(data, vennName, indexName, today):
    """The whole trackDb override map."""
    pairs = data["pairs"]
    strong = [p for p in pairs if p["how"] == "same name"]
    weak = [p for p in pairs if p["how"] == "tdbDefault"]
    comparable = [p for p in pairs if p["comparable"]]
    agree = [p for p in comparable if not p["missing"] and not p["extra"]]
    gaps = td.byMissingType(pairs, "same name")
    nGapSettings = len({p["setting"] for ps in gaps.values() for p in ps})

    return """<title>trackDb Override Map</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wght@500;600;700\
&family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Serif:ital,wght@0,400;0,600;1,400&display=swap">
<style>
%(css)s
</style>

<div class="wrap">

<header>
  <p class="eyebrow">UCSC Genome Browser &middot; trackDb and the cart</p>
  <h1>Which trackDb settings a user can change</h1>
  <p class="lede">A trackDb setting names a default. For some settings the browser also offers the
  reader a control, and the reader's choice lands in a cart variable. <strong>%(nPairs)d</strong>
  of the <strong>%(nSettings)d</strong> documented settings work that way.</p>
  <p class="lede">Both files also say which track types a setting applies to, and they were
  written from different evidence: the documentation by hand, the cart catalog by reading the
  config code. They disagree about <strong>%(nGapSettings)d</strong> settings.</p>
  <div class="sources">
    <div class="sourcecard tdb">
      <h3>trackDbSettings.json %(version)s</h3>
      <p class="big">%(nSettings)d</p>
      <p>Settings, generated from trackDbLibrary.shtml by <code class="t">make settings</code>.
      The <code class="t">types</code> list on each one is what the hub wizard publishes and what
      #37908 has been correcting.</p>
    </div>
    <div class="sourcecard cart">
      <h3>cartTrackVarCatalog #37838</h3>
      <p class="big">%(nCart)d</p>
      <p>Track cart variables, each filed under the config function that reads it, with the
      trackDb types that function serves. Built by reading hui.c and the per-type Ui functions,
      not by reading the documentation.</p>
    </div>
  </div>
</header>

<section>
  <h2>Types the config code serves and the documentation does not list</h2>
  <p class="sub">Each block is one type and the settings whose <code>types</code> list omits it.
  A block is usually one edit repeated across a family, which is how it is worth fixing. These are
  candidates, not verdicts: every row still has to be read against the code. The pairs below are
  joined by name, so the setting and the cart variable are one knob under two spellings.</p>
%(work)s
</section>

<section>
  <h2>Weaker candidates, joined through a default</h2>
  <p class="sub">Here the setting and the cart variable are spelled differently, and the join is
  the cart catalog's own note that the variable takes its default from that setting. A variable
  can be offered for a type whose default comes from somewhere else, so a type on this list is a
  question rather than a candidate.</p>
%(weakWork)s
</section>

<section>
  <h2>Every setting with a runtime override</h2>
  <p class="sub">All %(nPairs)d pairs. A type in the cart column that the docs do not list is
  marked. <strong>Not comparable</strong> means one side has nothing to say: a variable filed by
  track name or in a wildcard family carries no type list, and a setting that applies to every
  track cannot disagree about which types it covers.</p>
  <div class="tablewrap">
%(table)s
  </div>
</section>

<section>
  <h2>The two sides that did not join</h2>
  <p class="sub">Most of both files has no counterpart in the other, and that is the expected
  shape. A setting with no cart variable is one the browser reads and never offers to change. A
  cart variable with no setting is a control with no trackDb default behind it.</p>
  <div class="notes">
    <div class="notecard">
      <h3>%(nNoOverride)d settings a reader cannot change</h3>
      <p>They configure the track once, from trackDb, and the browser offers no control.</p>
    </div>
    <div class="notecard">
      <h3>%(nNoSetting)d cart variables with no documented setting</h3>
      <p>Controls whose value has no trackDb default, plus every variable the cart catalog files
      by track name or in a wildcard family.</p>
    </div>
  </div>
  <p></p>
  <details>
    <summary>The %(nNoOverride)d settings with no runtime override</summary>
%(noOverride)s
  </details>
  <p></p>
  <details>
    <summary>The %(nNoSetting)d cart variables with no trackDb setting</summary>
%(noSetting)s
  </details>
</section>

<section>
  <h2>How much to trust this</h2>
  <div class="notes">
    <div class="notecard">
      <h3>The cart catalog only knows types with a control</h3>
      <p>It files a variable under the config function that draws it, so it can only speak about
      types that have one. A type in the documentation and not in the cart column is usually the
      documentation being right about a type with no UI.</p>
    </div>
    <div class="notecard">
      <h3>A variable can sit in two groups</h3>
      <p>One variable read by two config functions collects the types of both.
      <code>aggregate</code> is filed under multiWig and under wig from the same source line, so
      the wig types on its row are the catalog being generous rather than the documentation being
      short.</p>
    </div>
    <div class="notecard">
      <h3>Both files can be wrong together</h3>
      <p>%(nAgree)d comparable pairs agree exactly. That is two independent readings landing in
      the same place, which is worth something, but neither was checked against a track that
      actually renders.</p>
    </div>
  </div>
</section>

<footer>
  Sources: trackDbSettings.json %(version)s, generated from trackDbLibrary.shtml &middot;
  cartTrackVarCatalog #37838<br>
  Built %(today)s against %(tree)s. Refs #37908 #37838.<br>
  The four registries and how they overlap: <a href="%(vennName)s">Four Config Registries</a>
  &middot; every name with its description: <a href="%(indexName)s">Registry Name Index</a><br>
  Drawn by hg/utils/registryPages/registryPages.py.
</footer>

</div>
""" % {
        "css": style("correlate.css"),
        "today": today,
        "tree": esc(shortPath(rd.kentSrc())),
        "version": esc(data["version"]),
        "nSettings": len(data["settings"]),
        "nCart": len(data["cart"]),
        "nPairs": len(pairs),
        "nAgree": len(agree),
        "nGapSettings": nGapSettings,
        "work": workList(pairs, "same name"),
        "weakWork": workList(pairs, "tdbDefault", weak=True),
        "table": pairTable(pairs),
        "nNoOverride": len(data["noOverride"]),
        "nNoSetting": len(data["noSetting"]),
        "noOverride": chipList(data["noOverride"]),
        "noSetting": chipList(data["noSetting"]),
        "vennName": esc(vennName),
        "indexName": esc(indexName),
    }


# ====================================================================== cli ==

def main():
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--outDir", help="write both pages into this directory")
    parser.add_argument("--venn", help="write the Venn page here")
    parser.add_argument("--index", help="write the name index here")
    parser.add_argument("--correlate", help="write the trackDb override map here")
    parser.add_argument("--check", action="store_true",
                        help="write nothing, just audit the shared names; for a cron")
    parser.add_argument("--audit", action="store_true",
                        help="also run sessionCartAudit, which needs the database")
    parser.add_argument("--date", help="date to stamp on the pages (default today)")
    parser.add_argument("--vennLink",
                        help="href each page uses to point at the Venn page "
                             "(default its file name, which is right when both sit in one "
                             "directory; give a full URL when they do not)")
    parser.add_argument("--indexLink", help="href each page uses to point at the name index")
    parser.add_argument("--correlateLink",
                        help="href the other pages use to point at the trackDb override map")
    args = parser.parse_args()

    if not (args.outDir or args.venn or args.index or args.correlate or args.check):
        parser.error("nothing to do: give --outDir, --venn, --index, --correlate or --check")

    regs = rd.loadRegistries()
    ok = rd.checkShared(regs)

    if args.check and not (args.outDir or args.venn or args.index or args.correlate):
        sys.exit(0 if ok else 1)

    counts = rd.regionCounts(regs)
    shared = rd.computeShared(regs)
    coll = rd.computeCollisions(regs)
    baseline = rd.baselineCounts()
    audit = rd.sessionAudit() if args.audit else None
    today = args.date or datetime.date.today().isoformat()

    vennPath = args.venn
    indexPath = args.index
    corrPath = args.correlate
    if args.outDir:
        vennPath = vennPath or os.path.join(args.outDir, "registryVenn.html")
        indexPath = indexPath or os.path.join(args.outDir, "registryIndex.html")
        corrPath = corrPath or os.path.join(args.outDir, "trackDbOverrides.html")

    vennName = args.vennLink or (os.path.basename(vennPath) if vennPath
                                 else "registryVenn.html")
    indexName = args.indexLink or (os.path.basename(indexPath) if indexPath
                                   else "registryIndex.html")
    corrName = args.correlateLink or (os.path.basename(corrPath) if corrPath
                                      else "trackDbOverrides.html")

    if vennPath:
        with open(vennPath, "w") as f:
            f.write(vennPage(regs, counts, shared, coll, baseline, audit, indexName, corrName,
                             today))
        print("wrote %s" % vennPath)
    if indexPath:
        with open(indexPath, "w") as f:
            f.write(indexPage(regs, shared, vennName, corrName, today))
        print("wrote %s" % indexPath)
    if corrPath:
        with open(corrPath, "w") as f:
            f.write(correlatePage(td.load(), vennName, indexName, today))
        print("wrote %s" % corrPath)

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
