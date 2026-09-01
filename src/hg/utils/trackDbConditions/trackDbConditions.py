#!/usr/bin/env python3
"""trackDbConditions.py - say what has to be true before a trackDb setting does anything.

Refs #37908.  The `types` list in trackDbLibrary.shtml says which track types a
setting applies to.  It is a flat list, so it cannot say "only in coverage
mode", "only when the track is in pack", "only when another setting is on".
harvestConditions.py next door finds those conditions in the C; this file sorts
them into kinds a person can act on, and reports them against the documented
settings.

Two scopes, kept apart:

    render   what has to be true for the setting to change the picture
    config   what has to be true for its control to appear on the config page

Usage:
    trackDbConditions.py --list                # every setting with a condition
    trackDbConditions.py --list --scope config
    trackDbConditions.py --setting autoScale   # one setting, with call sites
    trackDbConditions.py --kind coverage       # one kind of condition
    trackDbConditions.py --documented          # only settings trackDb documents
    trackDbConditions.py --json out.json
    trackDbConditions.py --check               # for a cron; see below

A condition is reported as *always* when it holds at every place the browser
reads that setting, and as *sometimes* when it holds at one place and not
another.  Only *always* is a claim about the setting; *sometimes* is a pointer
to a call site worth reading.

--check is the cron mode.  It fails when a setting the documentation describes
gains its first always-condition, or loses its last one, because either way the
documentation now says something different from the code.  The accepted state
lives in conditionBaseline.txt beside this file; --update-baseline accepts.
"""

import argparse
import collections
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harvestConditions as hc                                     # noqa: E402

BASELINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "conditionBaseline.txt")
SETTINGS_JSON = "hg/htdocs/goldenPath/help/trackDb/trackDbSettings.json"

# What kind of thing has to be true.  Order matters: the first match wins, and
# the earlier entries are the specific ones.
KINDS = [
    ("coverage", "density coverage mode",
     r"checkIfWiggling|winTooBigDoWiggle|limitWiggle|\bdoWiggle\b|setupForWiggle"),
    ("snake", "snake mode",
     r"\bdoSnake\b|snakeMode"),
    ("multiRegion", "multi-region mode",
     r"positionIsVirt|windows\s*->\s*next|slCount\(\s*windows|virtMode"),
    ("visibility", "the track's visibility",
     r"\btv(Full|Pack|Squish|Dense|Hide)\b|limitVisibility|->\s*visibility|\bvis\b\s*[!=]="),
    ("zoom", "the zoom level",
     r"zoomedTo\w+|basesPerPixel|winEnd\s*-\s*winStart|winTooBig|insideWidth"),
    ("trackType", "the track's type",
     r"(?:tdb|track|tg)\s*->\s*(?:tdb\s*->\s*)?type"
     r"|startsWithWord\(|(?:sameWord|sameString|startsWith)\s*\([^)]*\btype\b"),
    ("container", "where the track sits in a container",
     r"tdbIs(?:Composite|SuperTrack|MultiTrack|Subtrack|Container|Folder|View)\w*"
     r"|parentTdb|->\s*parent\b|subgroupingExists"),
    ("data", "what the data file contains",
     r"->\s*fieldCount|genotypeCount|->\s*itemCount|->\s*rowCount|bbi\s*->|wordCount"),
]

# Kinds a reader of the types list would not guess.  The track type is already
# what the list is about, and "other" needs a person, so neither counts.
SURPRISING = ("coverage", "snake", "multiRegion", "visibility", "zoom",
              "container", "data", "otherSetting")

OTHER_SETTING = "otherSetting"
OTHER_SETTING_TEXT = "another setting's value"
UNCLASSIFIED = "other"

READER_CALL = re.compile("|".join(re.escape(r) + r"\s*\(" for r in hc.READERS)
                         + r"|cartVarExistsAnyLevel\s*\(")


def accessorMap(reads):
    """Functions that exist only to read one setting, so a call to one is that setting.

    checkIfWiggling() is doWiggle, hicUiFetchAutoScale() is autoScale.  A
    condition that calls one of these is testing a setting, and saying so is
    more use than printing the function name.  The test is deliberately strict:
    the function must hold exactly one track-setting read, and that read must not
    itself sit behind a condition, or a large function with one incidental read
    would be mistaken for an accessor.
    """
    byFunc = collections.defaultdict(list)
    for read in reads:
        if read["tdb"] and read["func"]:
            byFunc[read["func"]].append(read)
    return {func: rs[0]["name"] for func, rs in byFunc.items()
            if len(rs) == 1 and not rs[0]["conds"]}


def classify(cond, accessors=None):
    """Which kind of condition this is, and the settings it names if any."""
    text = cond["text"]
    named = list(cond.get("derivedFrom") or [])
    for m in re.finditer(r'"([A-Za-z][A-Za-z0-9_.]*)"', text):
        if READER_CALL.search(text):
            named.append(m.group(1))
    for m in re.finditer(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", text):
        if accessors and m.group(1) in accessors:
            named.append(accessors[m.group(1)])
    if named:
        return OTHER_SETTING, sorted(set(named))
    for kind, _, pattern in KINDS:
        if re.search(pattern, text):
            return kind, []
    return UNCLASSIFIED, []


KIND_TEXT = dict([(k, t) for k, t, _ in KINDS]
                 + [(OTHER_SETTING, OTHER_SETTING_TEXT), (UNCLASSIFIED, "something else")])
KIND_ORDER = [k for k, _, _ in KINDS] + [OTHER_SETTING, UNCLASSIFIED]


def condId(cond):
    return re.sub(r"\s+", "", cond["text"])


def build(kentSrc=None, cache=None):
    """Harvest, classify, and fold the read sites together per setting and scope."""
    kentSrc = kentSrc or hc.os.environ.get("KENT_SRC") or os.path.abspath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
    if cache and os.path.exists(cache):
        with open(cache) as f:
            reads = json.load(f)
    else:
        reads = hc.harvest(kentSrc)
        if cache:
            with open(cache, "w") as f:
                json.dump(reads, f)

    accessors = accessorMap(reads)
    settings = {}
    for read in reads:
        if not read["tdb"]:
            continue                       # a page cart variable, not a track setting
        key = (read["name"], read["scope"])
        entry = settings.setdefault(key, {"name": read["name"], "scope": read["scope"],
                                          "sites": [], "unknown": False})
        conds = []
        for cond in hc.realConds(read):
            kind, named = classify(cond, accessors)
            conds.append({"text": cond["text"], "kind": kind, "names": named,
                          "file": cond["file"], "line": cond["line"]})
        entry["sites"].append({"file": read["file"], "line": read["line"],
                               "func": read["func"], "reader": read["reader"],
                               "conds": conds})
        if read["callerUnknown"]:
            entry["unknown"] = True

    for entry in settings.values():
        perSite = [{condId(c): c for c in s["conds"]} for s in entry["sites"]]
        always, sometimes = {}, {}
        if perSite:
            common = set(perSite[0])
            for one in perSite[1:]:
                common &= set(one)
            for one in perSite:
                for cid, cond in one.items():
                    (always if cid in common else sometimes)[cid] = cond
        entry["always"] = sorted(always.values(), key=lambda c: (KIND_ORDER.index(c["kind"]),
                                                                c["text"]))
        entry["sometimes"] = sorted(sometimes.values(), key=lambda c: (KIND_ORDER.index(c["kind"]),
                                                                      c["text"]))
    return list(settings.values())


def documented(kentSrc=None):
    """The settings trackDbLibrary.shtml describes, so the report can say which."""
    kentSrc = kentSrc or os.environ.get("KENT_SRC") or "."
    path = os.path.join(kentSrc, SETTINGS_JSON)
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        doc = json.load(f)
    return {s["key"]: s for s in doc["settings"]}


def baselineNames():
    if not os.path.exists(BASELINE):
        return set()
    with open(BASELINE) as f:
        return {ln.strip() for ln in f if ln.strip() and not ln.startswith("#")}


def conditionedNames(entries, docs, scope="render"):
    """Documented settings that carry an always-condition, as scope:name."""
    return {"%s:%s" % (e["scope"], e["name"]) for e in entries
            if e["scope"] == scope and e["always"] and e["name"] in docs}


def showSetting(entry, docs, verbose=False):
    doc = docs.get(entry["name"])
    types = doc["types"] if doc else None
    if isinstance(types, list):
        types = ", ".join(types)
    print("%s  [%s]" % (entry["name"], entry["scope"]))
    if doc:
        print("    documented for: %s" % (types or "?"))
    else:
        print("    not in trackDbLibrary.shtml")
    groups = [("always", entry["always"])]
    if verbose:
        groups.append(("sometimes", entry["sometimes"]))
    elif entry["sometimes"]:
        print("    (%d more conditions hold at some of its %d read sites; --verbose to see)"
              % (len(entry["sometimes"]), len(entry["sites"])))
    for label, conds in groups:
        for cond in conds:
            names = (" -> " + ", ".join(cond["names"])) if cond["names"] else ""
            print("    %-9s %-12s %s%s" % (label, cond["kind"], cond["text"][:88], names))
    if entry["unknown"]:
        print("    (some call paths could not be followed, so this is a floor)")
    if verbose:
        for site in entry["sites"]:
            print("      %s:%d  %s()" % (site["file"], site["line"], site["func"]))


def main():
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="every setting with a condition")
    parser.add_argument("--setting", help="just this setting")
    parser.add_argument("--kind", help="just this kind of condition (%s)"
                                       % ", ".join(KIND_ORDER))
    parser.add_argument("--scope", default="render", choices=list(hc.SCOPES) + ["both"])
    parser.add_argument("--documented", action="store_true",
                        help="only settings trackDbLibrary.shtml describes")
    parser.add_argument("--verbose", action="store_true",
                        help="show the sometimes-conditions and the call sites too")
    parser.add_argument("--surprising", action="store_true",
                        help="only settings whose condition is not just the track type")
    parser.add_argument("--json", help="write everything here")
    parser.add_argument("--check", action="store_true", help="cron mode, see the module docstring")
    parser.add_argument("--update-baseline", action="store_true",
                        help="accept the current set of conditioned settings")
    parser.add_argument("--cache", help="reuse a harvest written here")
    args = parser.parse_args()

    kentSrc = os.environ.get("KENT_SRC") or os.path.abspath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
    entries = build(kentSrc, args.cache)
    docs = documented(kentSrc)

    if args.json:
        with open(args.json, "w") as f:
            json.dump({"settings": entries, "kinds": KIND_TEXT}, f, indent=1)
        print("wrote %s" % args.json)

    if args.update_baseline:
        names = sorted(conditionedNames(entries, docs))
        with open(BASELINE, "w") as f:
            f.write("# Documented settings whose reads all sit behind a condition.\n"
                    "# Accepted state for trackDbConditions.py --check.  One scope:name per line.\n")
            for name in names:
                f.write(name + "\n")
        print("wrote %s (%d settings)" % (BASELINE, len(names)))
        return

    if args.check:
        now = conditionedNames(entries, docs)
        was = baselineNames()
        problems = 0
        for name in sorted(now - was):
            print("newly conditional, and the docs do not say so: %s" % name)
            problems += 1
        for name in sorted(was - now):
            print("no longer conditional, the note can go: %s" % name)
            problems += 1
        sys.exit(1 if problems else 0)

    scopes = list(hc.SCOPES) if args.scope == "both" else [args.scope]
    picked = [e for e in entries if e["scope"] in scopes]
    if args.documented:
        picked = [e for e in picked if e["name"] in docs]
    if args.setting:
        picked = [e for e in picked if e["name"] == args.setting]
        for entry in picked:
            showSetting(entry, docs, verbose=True)
        if not picked:
            print("no read of %s found in %s" % (args.setting, ", ".join(scopes)))
        return

    conditioned = [e for e in picked if e["always"] or e["sometimes"]]
    if args.kind:
        conditioned = [e for e in conditioned
                       if any(c["kind"] == args.kind for c in e["always"] + e["sometimes"])]
    if args.surprising:
        conditioned = [e for e in conditioned
                       if any(c["kind"] in SURPRISING for c in e["always"])]

    if args.list or args.kind or args.surprising:
        for entry in sorted(conditioned, key=lambda e: (e["scope"], e["name"].lower())):
            showSetting(entry, docs, args.verbose)
            print()

    inDocs = [e for e in conditioned if e["name"] in docs]
    always = [e for e in conditioned if e["always"]]
    print("settings read           %d  (%s)" % (len(picked), ", ".join(scopes)))
    print("with any condition      %d" % len(conditioned))
    print("with an always-condition %d" % len(always))
    print("of those, documented    %d" % len([e for e in always if e["name"] in docs]))
    print("documented and conditional at all: %d" % len(inDocs))
    print()
    tally = collections.Counter()
    for entry in conditioned:
        for cond in entry["always"]:
            tally[cond["kind"]] += 1
    print("always-conditions by kind:")
    for kind in KIND_ORDER:
        if tally[kind]:
            print("  %-14s %3d   %s" % (kind, tally[kind], KIND_TEXT[kind]))


if __name__ == "__main__":
    main()
