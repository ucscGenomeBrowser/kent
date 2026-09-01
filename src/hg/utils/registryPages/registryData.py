#!/usr/bin/env python3
"""registryData.py - read the four configuration catalogs and work out what they share.

Refs #37838, #37923, #37925, #37623.  Four catalogs under hg/utils each describe
one part of what can be configured in the browser:

    cartTrackVarCatalog   #37838   cart variables scoped to a track name
    urlCommandCatalog     #37923   parameters that can go on a CGI URL
    cartFileVarCatalog    #37623   cart variables that hold a server file name
    hgConfCatalog         #37925   settings the CGIs read out of hg.conf

Each one answers for its own surface and none of them knows the others exist.
This module reads all four, puts every row in one shape, and answers the
question none of them can: which names does more than one registry describe.

It is the data half of registryPages.py, which draws the two pages.  Nothing
here is hand-entered except KNOWN_SHARED below, and that exists to be checked
rather than trusted.

Every catalog is read through its own `--json`, so this module never parses a
catalog's Python.  All four honor $KENT_SRC.

Matching rule, and the reason it matters
----------------------------------------
The track catalog stores a variable as a suffix plus the separator that joins it
to the track name: name `heightPer` with sep `.` is the cart variable
`<track>.heightPer`.  The URL catalog spells the same variable out in full.  So
the two catalogs are compared on the **display form**, never on the bare name.

Comparing bare names is wrong in both directions.  It misses the four
track-scoped names, spelled `<track>_sel` in one catalog and `_sel` in the
other.  And it invents overlaps that are not there: `filter`, `start`, `end`,
`type` and `categories` are hubApi and hgBlat arguments on one side and
track-scoped suffixes on the other, and `geneTracks` is an hgFind string on one
side and a cart list on the other.  Those six are reported separately, as
collisions, because a bare name is not an identity.
"""

import json
import os
import subprocess
import sys
import tempfile

# Every name the four catalogs describe as one and the same variable, confirmed
# by reading the call sites.  computeShared() derives the same set from the
# catalogs; checkShared() compares the two and complains when they differ, so a
# name that starts or stops being shared has to be looked at by a person.
KNOWN_SHARED = {
    "<track>":               ("track", "url"),
    "<track>_sel":           ("track", "url"),
    "<track>_hideKids":      ("track", "url"),
    "<track>_imgOrd":        ("track", "url"),
    "<track>.heightPer":     ("track", "url"),
    "hgt.oligoMatch":        ("track", "url"),
    "dup_tracks":            ("track", "file"),
    "fileUrl":               ("url", "file"),
    "hgS_loadUrlName":       ("url", "file"),
    "multiRegionsBedUrl":    ("url", "file"),
    "textSize":              ("url", "conf"),
}

# The order the registries are drawn in, and the color slot each one owns.
REG_ORDER = ("track", "url", "file", "conf")

REG_TITLE = {
    "track": "Track cart variables",
    "url":   "URL parameters",
    "file":  "File cart variables",
    "conf":  "hg.conf settings",
}

REG_TICKET = {"track": "37838", "url": "37923", "file": "37623", "conf": "37925"}

REG_TOOL = {
    "track": "cartTrackVarCatalog",
    "url":   "urlCommandCatalog",
    "file":  "cartFileVarCatalog",
    "conf":  "hgConfCatalog",
}

REG_BLURB = {
    "track": "Cart variables scoped to one track name. The catalog sorts them into levels, from "
             "the bare track name down to the wildcard families.",
    "url":   "Everything that can go on a browser CGI URL. An action is consumed and gone; a "
             "setting is written into the session and stays.",
    "file":  "Cart variables that hold the name of a file the server made, which a CGI later "
             "opens. The verdict says whether cart.c screens the value on the way in.",
    "conf":  "Settings the CGIs read out of hg.conf. A gate is a release flag meant to be "
             "deleted; a knob is a switch a mirror may set forever.",
}

# Floor on how many rows a working read finds, well under the real count.  Pointed
# at an empty tree or the wrong KENT_SRC every count below comes out zero and the
# pages would render as a set of empty circles.
MIN_ROWS = {"track": 250, "url": 250, "file": 15, "conf": 200}


def kentSrc():
    """Root of the kent source tree.  $KENT_SRC wins, else the tree this file sits in."""
    src = os.environ.get("KENT_SRC")
    if src:
        return os.path.abspath(src)
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.abspath(os.path.join(here, "..", "..", ".."))


def catalogJson(key, src=None):
    """Run one catalog's --json and return what it wrote."""
    src = src or kentSrc()
    tool = REG_TOOL[key]
    prog = os.path.join(src, "hg", "utils", tool, tool + ".py")
    if not os.path.exists(prog):
        raise SystemExit("cannot find %s\nis KENT_SRC right?  it is %s" % (prog, src))
    env = dict(os.environ, KENT_SRC=src)
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, key + ".json")
        run = subprocess.run([sys.executable, prog, "--json", out],
                             env=env, capture_output=True, text=True)
        if run.returncode != 0 or not os.path.exists(out):
            raise SystemExit("%s --json failed:\n%s" % (tool, run.stderr.strip()))
        with open(out) as f:
            return json.load(f)


def meta(*parts):
    """One metadata line from whichever fields the catalog filled in."""
    return " · ".join([p for p in parts if p])


def row(name, desc, metaLine, bare=None):
    """One catalog row.  `bare` is the catalog's own spelling when it differs from `name`."""
    return {"name": name, "desc": desc or "", "meta": metaLine or "",
            "bare": name if bare is None else bare}


# ---------------------------------------------------------------- hg.conf ----

def buildConf(cat):
    """hg.conf settings, in the catalog's own sections."""
    groups = []
    for sec in cat["sections"]:
        rows = []
        for v in sec["vars"]:
            tickets = v.get("tickets") or ([v["ticket"]] if v.get("ticket") else [])
            rows.append(row(v["name"], v.get("note", ""), meta(
                v.get("kind"),
                v.get("role"),
                "default %s" % v["default"] if v.get("default") is not None else None,
                "env %s" % v["env"] if v.get("env") else None,
                "deprecated" if v.get("deprecated") else None,
                v.get("src"),
                " ".join("#%s" % t for t in tickets) or None)))
        groups.append({"title": sec["title"], "what": sec.get("what", ""), "rows": rows})
    return groups


# ------------------------------------------------------------------- URL ----

def urlRows(cmd):
    """One URL catalog entry, plus the members of a prefix family like hgta_do*."""
    how = "persists in the cart" if cmd.get("persists") else "not persisted"
    out = [row(cmd["name"], cmd.get("note", ""), meta(
        cmd.get("kind"),
        "value %s" % cmd["value"] if cmd.get("value") else None,
        how,
        "documented publicly" if cmd.get("public") else None,
        cmd.get("src")))]
    for member in cmd.get("members", []):
        out.append(row(member, cmd.get("note", ""), meta(
            cmd.get("kind"), "member of %s" % cmd["name"], how, cmd.get("src"))))
    return out


def buildUrl(cat):
    """URL parameters: the shared sections, then the per-CGI ones, then hubApi."""
    groups = []
    for sec in cat["sections"]:
        rows = [r for c in sec["cmds"] for r in urlRows(c)]
        groups.append({"title": sec["title"], "what": sec.get("what", ""), "rows": rows})
    for cgi, info in cat.get("otherCgis", {}).items():
        rows = [r for c in info.get("cmds", []) for r in urlRows(c)]
        if rows:
            groups.append({"title": cgi, "what": info.get("what", ""), "rows": rows})
    api = cat.get("hubApi", {})
    rows = [r for c in (api.get("cmds") or api.get("args") or [])
            if isinstance(c, dict) for r in urlRows(c)]
    if rows:
        groups.append({"title": "hubApi", "what": api.get("what", ""), "rows": rows})
    return groups


# ---------------------------------------------------- track cart variables ----

LEVEL_TITLE = {
    "1_trackName":    "Level 1 · the track name",
    "2_common":       "Level 2 · every track",
    "2b_container":   "Level 2b · containers",
    "3_byType":       "Level 3 · by track type",
    "3b_byTrackName": "Level 3b · by track name",
    "4_families":     "Level 4 · wildcard families",
}


def trackDisplay(v):
    """The cart variable a track catalog row describes, spelled out in full.

    The catalog stores the suffix and the separator that joins it to the track
    name.  A name that already carries the separator does not get a second one,
    and the one row with no name at all is the bare track name, the visibility.
    """
    name, sep = v.get("name", ""), v.get("sep", "")
    if sep == "cgs_<track>_":
        return "cgs_<track>_" + name.lstrip("_")
    if sep in (".", "_"):
        return "<track>" + name if name.startswith(sep) else "<track>" + sep + name
    return name if name else "<track>"


def trackRow(v, groupWhat):
    """A track variable, falling back to its group's description when it has no note."""
    return row(trackDisplay(v), v.get("note") or groupWhat or "", meta(
        v.get("type"),
        "one of: %s" % ", ".join(str(x) for x in v["values"]) if v.get("values") else None,
        "default %s" % v["default"] if v.get("default") is not None else None,
        v.get("src")),
        bare=v.get("name", ""))


def buildTrack(cat):
    """Track cart variables: the levels, then the strays the levels cannot hold."""
    groups = []

    lv = cat["levels"]["1_trackName"]
    groups.append({
        "title": LEVEL_TITLE["1_trackName"], "what": lv.get("what", ""),
        "rows": [row(p["prefix"] + "<track>",
                     "The track-name prefix for a %s." % p.get("what", "track"),
                     meta("prefix", p.get("src"))) for p in lv.get("prefixes", [])]})

    for key in ("2_common", "2b_container", "3_byType", "3b_byTrackName", "4_families"):
        level = cat["levels"][key]
        sub = level.get("groups") or level.get("types") or level.get("tracks") or {}
        for name, group in sub.items():
            rows = [trackRow(v, group.get("what", "")) for v in group.get("vars", [])]
            if rows:
                groups.append({"title": "%s → %s" % (LEVEL_TITLE[key], name),
                               "what": group.get("what", ""), "rows": rows})

    notScoped = cat.get("notScopedByTrackName", {})
    if notScoped.get("vars"):
        groups.append({"title": "Not scoped by track name", "what": notScoped.get("what", ""),
                       "rows": [trackRow(v, "") for v in notScoped["vars"]]})

    for name, group in cat.get("otherCgis", {}).get("groups", {}).items():
        rows = [trackRow(v, group.get("what", "")) for v in group.get("vars", [])]
        if rows:
            groups.append({"title": "Other CGIs → " + name,
                           "what": group.get("what", ""), "rows": rows})

    # The exceptions list names variables that configure a track without fitting
    # the <track><sep><var> shape.  One pattern can spell out several names.
    rows = []
    for e in cat.get("exceptions", []):
        for name in [x.strip() for x in e["pattern"].split(",") if x.strip()]:
            what = e.get("what", "").rstrip(".")
            rows.append(row(name, ("%s. %s" % (what, e.get("why", ""))).strip(),
                            meta("exception", e.get("src"))))
    if rows:
        groups.append({
            "title": "Exceptions to the naming shape",
            "what": "Cart variables that configure a track but do not fit the "
                    "<track><sep><var> shape, so no accessor keyed on the track name can "
                    "reach them.",
            "rows": rows})
    return groups


# ----------------------------------------------------- file cart variables ----

VERDICT_TITLE = {
    "screened":     "Screened by cart.c",
    "notBuilt":     "Not screened, and the cart value never becomes a path",
    "trackDbNamed": "Named by trackDb rather than by the cart",
    "otherCheck":   "Screened by a check of its own",
}


def buildFile(cat):
    """File-name cart variables, grouped by the catalog's verdict."""
    byVerdict = {}
    order = []
    for e in cat:
        if e["verdict"] not in byVerdict:
            byVerdict[e["verdict"]] = []
            order.append(e["verdict"])
        byVerdict[e["verdict"]].append(e)
    groups = []
    for verdict in order:
        rows = [row(e["name"], e.get("note", ""), meta(
            "cart.c screen: %s" % e["screen"] if e.get("screen") else "no cart.c screen",
            "opened by %s" % e["sink"] if e.get("sink") else None,
            e.get("where"))) for e in byVerdict[verdict]]
        groups.append({"title": VERDICT_TITLE.get(verdict, verdict), "what": "", "rows": rows})
    return groups


BUILDERS = {"conf": buildConf, "url": buildUrl, "track": buildTrack, "file": buildFile}


# --------------------------------------------------------------- the whole ----

def loadRegistries(src=None):
    """Read all four catalogs.  Returns them in REG_ORDER, each with its groups."""
    src = src or kentSrc()
    regs = []
    for key in REG_ORDER:
        groups = BUILDERS[key](catalogJson(key, src))
        nRows = sum(len(g["rows"]) for g in groups)
        if nRows < MIN_ROWS[key]:
            raise SystemExit("%s gave only %d rows, expected at least %d.  "
                             "Is KENT_SRC (%s) pointing at a real tree?"
                             % (REG_TOOL[key], nRows, MIN_ROWS[key], src))
        regs.append({
            "key": key, "title": REG_TITLE[key], "ticket": REG_TICKET[key],
            "tool": REG_TOOL[key], "blurb": REG_BLURB[key], "groups": groups,
            "rows": nRows,
            "names": sorted({r["name"] for g in groups for r in g["rows"]}),
        })
    return regs


def computeShared(regs):
    """Names more than one registry describes, matched on the full cart variable name.

    Returns {name: (regKey, ...)} in REG_ORDER.
    """
    where = {}
    for reg in regs:
        for name in reg["names"]:
            where.setdefault(name, []).append(reg["key"])
    return {n: tuple(k for k in REG_ORDER if k in ks)
            for n, ks in where.items() if len(ks) > 1}


def computeCollisions(regs):
    """Names two registries spell the same way while meaning different variables.

    A track catalog row's own spelling is a suffix, so its full name carries the
    <track> scope.  When the suffix alone matches another registry's parameter,
    the two are different variables that happen to share a word.
    Returns [(bareName, {regKey: [fullName, ...]})].
    """
    bare = {}
    for reg in regs:
        for group in reg["groups"]:
            for r in group["rows"]:
                bare.setdefault(r["bare"], {}).setdefault(reg["key"], set()).add(r["name"])
    shared = computeShared(regs)
    out = []
    for name in sorted(bare):
        if not name or len(bare[name]) < 2:
            continue
        if name in shared:              # same spelling and the same variable
            continue
        out.append((name, {k: sorted(v) for k, v in bare[name].items()}))
    return out


def checkShared(regs, out=sys.stderr):
    """Compare the shared names against KNOWN_SHARED.  Returns True when they agree.

    A difference is not an error in the pages, which are drawn from the catalogs
    either way.  It means a name started or stopped being shared and somebody has
    to read the call sites and update KNOWN_SHARED.
    """
    found = computeShared(regs)
    ok = True
    for name in sorted(set(found) - set(KNOWN_SHARED)):
        print("shared by %s and not classified: %s" % ("+".join(found[name]), name), file=out)
        ok = False
    for name in sorted(set(KNOWN_SHARED) - set(found)):
        print("no longer shared, drop from KNOWN_SHARED: %s" % name, file=out)
        ok = False
    for name in sorted(set(found) & set(KNOWN_SHARED)):
        if found[name] != tuple(k for k in REG_ORDER if k in KNOWN_SHARED[name]):
            print("shared by different registries now: %s is in %s"
                  % (name, "+".join(found[name])), file=out)
            ok = False
    return ok


def regionCounts(regs):
    """How many names sit in each region of the four-set diagram.

    Keys are tuples of registry keys in REG_ORDER, so ("url",) is the names only
    the URL catalog has and ("track", "url") is the ones both describe.
    """
    where = {}
    for reg in regs:
        for name in reg["names"]:
            where.setdefault(name, []).append(reg["key"])
    counts = {}
    for name, keys in where.items():
        region = tuple(k for k in REG_ORDER if k in keys)
        counts[region] = counts.get(region, 0) + 1
    return counts


def baselineCounts(src=None):
    """Rows in the two committed baselines of names deliberately left out of scope."""
    src = src or kentSrc()
    out = {}
    for key, path in (("url", "urlCommandCatalog/urlNamesNotCataloged.txt"),
                      ("track", "cartTrackVarCatalog/cartVarsNotCataloged.txt")):
        full = os.path.join(src, "hg", "utils", path)
        n = 0
        if os.path.exists(full):
            with open(full) as f:
                n = sum(1 for line in f if line.strip() and not line.startswith("#"))
        out[key] = n
    return out


def sessionAudit(src=None):
    """Ask sessionCartAudit what the real saved sessions hold.  Needs the database.

    Returns {sessions, names, unknown} or None when the audit cannot run.
    """
    src = src or kentSrc()
    prog = os.path.join(src, "hg", "utils", "sessionCartAudit", "sessionCartAudit.py")
    if not os.path.exists(prog):
        return None
    env = dict(os.environ, KENT_SRC=src)
    try:
        run = subprocess.run([sys.executable, prog, "--check"],
                             env=env, capture_output=True, text=True, timeout=900)
    except (subprocess.TimeoutExpired, OSError):
        return None
    if run.returncode != 0:
        return None
    got = {}
    for line in run.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0] == "sessions" and parts[1] == "read":
            got["sessions"] = int(parts[2])
        elif len(parts) >= 3 and parts[0] == "distinct" and parts[1] == "names":
            got["names"] = int(parts[2])
        elif len(parts) >= 2 and parts[0] == "unknown":
            got["unknown"] = int(parts[1])
    return got if len(got) == 3 else None


def main():
    """Print what the four catalogs share.  registryPages.py draws the pages."""
    regs = loadRegistries()
    for reg in regs:
        print("%-6s %4d rows  %4d names  %3d groups  %s"
              % (reg["key"], reg["rows"], len(reg["names"]), len(reg["groups"]), reg["tool"]))
    shared = computeShared(regs)
    print("\n%d names in more than one registry:" % len(shared))
    for name in sorted(shared):
        print("  %-24s %s" % (name, "+".join(shared[name])))
    coll = computeCollisions(regs)
    print("\n%d spellings that collide without being the same variable:" % len(coll))
    for name, where in coll:
        print("  %-24s %s" % (name, "  ".join(
            "%s:%s" % (k, ",".join(v)) for k, v in sorted(where.items()))))
    sys.exit(0 if checkShared(regs) else 1)


if __name__ == "__main__":
    main()
