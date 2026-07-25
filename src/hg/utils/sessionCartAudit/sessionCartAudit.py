#!/usr/bin/env python3
"""sessionCartAudit.py - check the cart catalogs against the sessions people saved.

Refs #37838 and #37923.  Third sibling of hg/utils/cartTrackVarCatalog (which
catalogs track-scoped cart variables) and hg/utils/urlCommandCatalog (which
catalogs URL parameters).  Both of those were built by reading the source.  This
one reads the other direction: it pulls every named session out of
namedSessionDb, takes apart the stored var=val blob, and asks which of the names
in it neither catalog can account for.

Why it is worth having.  A catalog derived from the source can only be as
complete as the greps that built it.  It cannot see a variable that only
JavaScript writes, it cannot see a name assembled at run time from a track name
plus a field name, and it cannot tell you which of the leaks it predicted are
actually sitting in somebody's saved session.  The sessions can.  They are a
transcript of everything the browser has ever been asked to remember.

  cartTrackVarCatalog.py   source  -> what a track variable may be called
  urlCommandCatalog.py     source  -> what may go on a URL
  sessionCartAudit.py      data    -> what is actually in the carts, and which
                                      of it the other two do not cover

The catch, and the reason the matching below is fussier than it looks: the
track catalog contains bare wildcard entries (<species>, <filterName>,
<wigTrack>.<wigVar>, <attribute><Value>, <view>.<anyTypeVar>, _<origin>) that
match literally any token.  Score them as matches and every unrecognised name
in the corpus is absorbed, the audit comes back clean, and it has proved
nothing.  They are matched separately and reported as "wildcard only", which is
the honest answer: covered by a catch-all, not actually catalogued.

Matching is right-anchored - longest known suffix at a '.' or '_' boundary -
never left-anchored at the first separator.  That is not a style choice.  Track
and species names in the real data contain dots (GenArk accessions such as
GCF_020740605.2), slashes, spaces and parentheses, so the left-hand side cannot
be delimited by anything except a complete list of what the right-hand side may
be.

Usage:
    sessionCartAudit.py --check                 # coverage counts
    sessionCartAudit.py --unknown               # names neither catalog knows
    sessionCartAudit.py --wildcard              # names only a catch-all matches
    sessionCartAudit.py --leaks                 # #37923 leak claims vs the data
    sessionCartAudit.py --findings              # the curated findings, with live counts
    sessionCartAudit.py --json out.json
    sessionCartAudit.py --html out.html

    --dump FILE     read a previously saved contents dump instead of the db
    --save-dump F   write the raw dump so later runs can skip the query
    --central DB    hgcentral database to read (default from hg.conf)

Reading the sessions takes a minute and a couple of hundred MB of query output,
so --save-dump / --dump is worth using while iterating.
"""

import argparse
import collections
import html
import importlib.util
import json
import os
import re
import subprocess
import sys
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
UTILS = os.path.dirname(HERE)

VIS_VALUES = {"hide", "dense", "squish", "pack", "full", "show", ""}
PLACEHOLDER = re.compile(r"<[a-zA-Z]+>")


# ---------------------------------------------------------------- the sessions

def defaultCentral():
    """Read central.db out of ~/.hg.conf, the same place the CGIs get it."""
    path = os.path.expanduser("~/.hg.conf")
    try:
        for line in open(path):
            if line.startswith("central.db="):
                return line.split("=", 1)[1].strip()
    except IOError:
        pass
    return "hgcentraltest"


def dumpSessions(central):
    """One row per named session, the raw contents blob.

    mysql batch mode escapes embedded newlines, so a row is a line.  The blob
    itself is CGI-encoded, so & and = inside a value arrive as %26 and %3D and
    the split below is safe."""
    cmd = ["hgsql", "-N", "-B", central, "-e",
           "select contents from namedSessionDb"]
    out = subprocess.run(cmd, stdout=subprocess.PIPE, check=True).stdout
    return out.decode("utf8", "replace")


def extract(text, sampleMax=4):
    """text -> {name: [sessionCount, totalCount, [sample values]]}, sessionCount."""
    counts = collections.Counter()
    inSessions = collections.Counter()
    samples = collections.defaultdict(set)
    nSess = 0

    for line in text.split("\n"):
        if not line:
            continue
        nSess += 1
        seen = set()
        for pair in line.split("&"):
            if not pair:
                continue
            rawName, _, rawVal = pair.partition("=")
            name = urllib.parse.unquote(rawName.replace("+", " "))
            counts[name] += 1
            seen.add(name)
            if len(samples[name]) < sampleMax:
                samples[name].add(urllib.parse.unquote(rawVal.replace("+", " "))[:60])
        for name in seen:
            inSessions[name] += 1

    names = {}
    for name, total in counts.items():
        names[name] = [inSessions[name], total, sorted(samples[name])]
    return names, nSess


# ---------------------------------------------------------------- the catalogs

def loadSibling(subdir, module):
    """Import a sibling catalog by path so this works from any cwd."""
    path = os.path.join(UTILS, subdir, module + ".py")
    if not os.path.exists(path):
        sys.exit("cannot find sibling catalog %s\n"
                 "expected it next door at %s" % (module, path))
    spec = importlib.util.spec_from_file_location(module, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def trackVarNames():
    """Every variable name in the #37838 catalog, however deeply nested."""
    mod = loadSibling("cartTrackVarCatalog", "cartTrackVarCatalog")
    cat = mod.build()
    names = set()

    def walk(node):
        if isinstance(node, dict):
            name = node.get("name")
            if isinstance(name, str) and ("type" in node or "src" in node):
                names.add(name)
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(cat)
    return names


def urlCommandNames():
    """Every parameter name in the #37923 catalog, plus the ones it calls leaks."""
    mod = loadSibling("urlCommandCatalog", "urlCommandCatalog")
    cat = mod.build()
    names, leaks = set(), set()

    def walk(node):
        if isinstance(node, dict):
            name = node.get("name")
            if isinstance(name, str) and ("kind" in node or "value" in node):
                names.add(name)
                if node.get("leaks"):
                    leaks.add(name)
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(cat)
    return names, leaks


def compilePatterns(catVars):
    """Split the catalog into literals, real families and bare wildcards.

    A family pattern still has something to anchor on once the <placeholders>
    come out (filter.<field> keeps "filter.").  A bare wildcard does not
    (<species> keeps nothing), so it would match any token at all and has to be
    kept out of the ordinary match path."""
    literals, families, wildcards = set(), [], []
    for var in catVars:
        if not PLACEHOLDER.search(var):
            literals.add(var)
            continue
        parts = PLACEHOLDER.split(var)
        regex = re.compile("^" + "[^.]+".join(re.escape(p) for p in parts) + "$")
        residue = PLACEHOLDER.sub("", var).strip("._")
        if len(residue) < 3:
            wildcards.append((var, regex))
        else:
            families.append((var, regex))
    return literals, families, wildcards


# ---------------------------------------------------------------- classify

class Audit(object):

    def __init__(self, names, nSess):
        self.names = names
        self.nSess = nSess
        self.catVars = trackVarNames()
        self.urlNames, self.catLeaks = urlCommandNames()
        self.literals, self.families, self.wildcards = compilePatterns(self.catVars)
        # Track-name vocabulary learned from the corpus itself: a bare name
        # whose every observed value is a visibility word is a track.
        self.trackNames = set(n for n, v in names.items()
                              if v[2] and all(x in VIS_VALUES for x in v[2]))
        self.buckets = collections.defaultdict(list)
        self.wildcardSuffix = collections.Counter()
        self._dbs = None
        self.classify()

    def sess(self, name):
        return self.names[name][0] if name in self.names else 0

    def dbVocabulary(self):
        """Assembly names, learned from the position.<db> variables in the corpus.

        Every session that has ever been at an assembly leaves one, so this is a
        better list than anything hard-coded, and it comes from the same data
        being audited."""
        if self._dbs is None:
            self._dbs = set(n.split(".", 1)[1] for n in self.names
                            if n.startswith("position.") and n.count(".") == 1)
        return self._dbs

    def famMatch(self, suffix, patterns):
        for src, regex in patterns:
            if regex.match(suffix):
                return src
        return None

    def peel(self, name):
        """Longest catalogued suffix at a separator. -> (stem, suffix, how)"""
        fallback = None
        for i in range(len(name) - 1):
            if name[i] not in "._":
                continue
            suffix = name[i + 1:]
            if suffix in self.literals or (name[i] + suffix) in self.literals:
                return name[:i], suffix, "literal"
            hit = self.famMatch(suffix, self.families)
            if hit:
                return name[:i], suffix, hit
            if fallback is None:
                hit = self.famMatch(suffix, self.wildcards)
                if hit:
                    fallback = (name[:i], suffix, "wildcard:" + hit)
        return fallback

    def classify(self):
        for name in self.names:
            if name in self.urlNames:
                self.buckets["urlKnown"].append(name)
                continue
            peeled = self.peel(name)
            if peeled and not peeled[2].startswith("wildcard:"):
                self.buckets["trackKnown"].append(name)
                continue
            if name in self.trackNames:
                self.buckets["trackVis"].append(name)
                continue
            if peeled:
                self.buckets["wildcardOnly"].append(name)
                self.wildcardSuffix[peeled[1]] += 1
                continue
            self.buckets["unknown"].append(name)

    def counts(self):
        return collections.OrderedDict(
            (k, len(self.buckets[k])) for k in
            ("trackVis", "trackKnown", "wildcardOnly", "urlKnown", "unknown"))

    def leakReport(self):
        seen = sorted(((self.sess(n), n) for n in self.catLeaks if n in self.names),
                      reverse=True)
        unseen = sorted(self.catLeaks - set(self.names))
        return seen, unseen

    def sorted_(self, bucket):
        rows = [(self.names[n][0], self.names[n][1], n, self.names[n][2])
                for n in self.buckets[bucket]]
        rows.sort(reverse=True)
        return rows


# ---------------------------------------------------------------- findings
#
# The prose is curated, the numbers are not.  Each finding carries a probe that
# recomputes its counts from the corpus, so the write-up cannot drift away from
# the data the way a hand-typed number would.  Same division of labour as the
# two sibling catalogs: a human says what it means, the script says how much.

def nameStats(audit, pred):
    hits = [n for n in audit.names if pred(n)]
    if not hits:
        return {"names": 0, "sessions": 0, "busiest": None, "busiestSess": 0}
    # tie-break on the name so repeated runs report the same example
    busiest = max(hits, key=lambda n: (audit.names[n][0], n))
    return {"names": len(hits),
            "sessions": sum(audit.names[n][0] for n in hits),
            "busiest": busiest,
            "busiestSess": audit.names[busiest][0]}


def dbSuffixFamily(audit, var):
    """Names of the form <var>_<db>.

    The suffix has to be checked against a real assembly list.  Left to just
    startswith(), hgPcrResult_imgOrd counts as an assembly named imgOrd, which
    is the very ambiguity this finding is about."""
    dbs = audit.dbVocabulary()
    return nameStats(audit, lambda n: (n.startswith(var + "_")
                                       and n[len(var) + 1:] in dbs))


def innerPositionCarts(audit, text=None):
    """The var=val blob stored inside the value of position.<db>."""
    inner = collections.Counter()
    forms = collections.Counter()
    dbs = set()
    if text is None:
        return inner, forms, dbs
    for match in re.finditer(r"position\.([A-Za-z0-9_]+)=([^&]*)", text):
        dbs.add(match.group(1))
        value = urllib.parse.unquote(match.group(2))
        if "=" not in value:
            forms["bare position"] += 1
            continue
        forms["nested cart"] += 1
        for pair in value.split("&"):
            inner[pair.partition("=")[0]] += 1
    return inner, forms, dbs


def labelFields(audit):
    fields = collections.Counter()
    for name in audit.names:
        match = re.search(r"\.label\.([A-Za-z0-9_]+)$", name)
        if match:
            fields[match.group(1)] += audit.names[name][0]
    catalogued = set(v.split(".", 1)[1] for v in audit.catVars
                     if v.startswith("label.") and "<" not in v)
    return fields, catalogued


NOISE_CLASSES = [
    ("Form buttons",
     "The name of the button the user clicked, saved as cart state.",
     lambda n: re.search(r"(Submit|Button)$", n) is not None),
    ("HTML-escaped ampersands",
     "A link copied out of an email or a web page, so &amp; became part of the "
     "next parameter's name.  These reach inside the nested position.<db> blob "
     "too, so the damage is stored two levels down.",
     lambda n: n.startswith("amp;")),
    ("Question mark glued to a name",
     "A second ? in the URL, or a hand-built link that lost its separator.",
     lambda n: "?" in n),
    ("Names containing a space",
     "Usually a track or species name with a stray space in it, which no "
     "left-to-right parse survives.",
     lambda n: " " in n),
    ("Non-ASCII bytes in the name",
     "Corruption rather than input.",
     lambda n: any(ord(ch) > 126 for ch in n)),
    ("Empty name",
     "A bare = in the query string.",
     lambda n: n == ""),
]

TYPOS = ["measureTIming", "meassureTiming", "measureTiminjf", "udcTimout",
         "udcTimeour", "ucbTimeout", "hdcTimout", "UDCtimeout", "emAltHighligh",
         "hgTracksCOnfigurePage", "ignoreCookies", "intereraction", "executeQury",
         "disableAdvancedJavascript", "hgTracksConfigurePage"]

# Global cart variables that belong to no track and are not documented URL
# parameters.  Neither catalog has a section for these.  Grouped by what they
# are for, because the groups behave differently: the first is user preference
# worth sharing, the second is view state the browser recomputes anyway, the
# last is one-request form scratch that should never have been saved.
GLOBAL_GROUPS = [
    ("Configure page display settings",
     "hg/hgTracks/config.c, read back in hgTracks.c:10620ff",
     ["leftLabels", "centerLabels", "trackControlsOnMain", "nextItemArrows",
      "nextExonArrows", "exonNumbers", "ideogram", "enableHighlightingDialog",
      "showDinkButtons", "doHgcInPopUp", "textFont", "textStyle", "fontType",
      "lineWidth", "tooltipTextSize", "theme", "displaySubtracks"]),
    ("Position and navigation state",
     "hg/hgTracks/hgTracks.c",
     ["lastPosition", "oldPosition", "dinkL", "dinkR", "rulerBaseZoom",
      "newWinWidth", "winStart", "winEnd", "chromName", "insideX",
      "rulerClickHeight", "dragSelection", "dragZooming", "prevHlColor"]),
    ("Multi-region state",
     "hg/hgTracks/hgTracks.c, hg/lib/web.c",
     ["virtMode", "virtModeType", "lastVirtModeType", "lastVirtModeExtraState",
      "emPadding", "emAltHighlight", "gmPadding", "singleAltHaploId",
      "virtWinFull", "autoRearr"]),
    ("Hub state",
     "hg/lib/hubConnect.c",
     ["trackHubs", "assumesHub", "hubText", "urlHub", "hubSearchTerms",
      "hubDbFilter", "tsIncludePublicHubs"]),
    ("Track search",
     "hg/hgTracks/searchTracks.c",
     ["tsGroup", "tsName", "tsDescr", "tsCurTab", "tsSimple", "tsType", "tsSort"]),
    ("Housekeeping",
     "hg/lib/cart.c, hg/inc/cart.h",
     ["defaultsSet", "cartVersion", "hgS_DataTableState", "hgPS_DataTableState",
      "sessionTable_length"]),
]

# The four ways the tree scopes a variable to an assembly.  None of them is in
# either catalog, and the first cannot be told apart from the legacy
# <track>_<var> form by parsing - only by knowing which half is the variable.
DB_SCOPES = [
    ("<var>_<db>", "hg/inc/cart.h:49 _cartVarDbName()", "ctfile_hg38"),
    ("position.<db>", "hg/lib/cart.c:4076 cartSetDbPosition()", "position.hg38"),
    ("customComposite-<db>", "hg/lib/sessionData.c:452", "customComposite-hg38"),
    ("hubQuickLift-<db>", "hg/inc/quickLift.h:9, sessionData.c:452", "hubQuickLift-hg38"),
]

DB_SUFFIX_VARS = [
    ("ctfile", "pointer to the user's custom track file for that assembly "
               "(customTrack.c:918); there is a hub form too, ctfile_hub_<id>, "
               "cart.c:1415"),
    ("complement", "complement the bases by the ruler (hui.h:117)"),
    ("hgt.baseTitle", "title over the base position track"),
    ("hgt.revCmplDisp", "reverse-complement the whole view"),
    ("hgGenome_threshold", "Genome Graphs significance threshold"),
    ("mvCtfile", "myVariants custom track file"),
    ("hgPcrResult", "in-cart PCR result"),
    ("hgSearch_categs", "search result categories"),
]

# Track-scoped names the #37838 catalog does not carry.  Each probe is a
# predicate over the corpus so the counts stay live.
MISSING_TRACK_VARS = [
    ("<track>.showCfg",
     lambda n: n.endswith(".showCfg"),
     "Per-view config disclosure, written at hg/lib/hui.c:8682 and read at "
     "hui.c:8718.  Also appears three deep as <composite>.<view>.showCfg."),
    ("<track>.childShowCfg",
     lambda n: n.endswith(".childShowCfg"),
     "Exists only in JavaScript, hg/js/hui.js:645.  No C code anywhere mentions "
     "it, which is exactly why a harvester that reads C never saw it."),
    ("<track>.label.<field>",
     lambda n: ".label." in n,
     "hui.c:4743 composes label.%s from whatever trackDb labelFields names, so "
     "the field set is open-ended by construction and cannot be enumerated.  It "
     "has to be a family."),
    ("<track>.<field>FilterMin / Max",
     lambda n: re.search(r"Filter(Min|Max)$", n) is not None,
     "The camel twins of filter.<field>Min/Max.  The catalog has "
     "<field>FilterLimits and friends but not these."),
    ("<track>.filterBy.<a.b>",
     lambda n: re.search(r"\.(filter|filterBy|highlightBy)\.[^.]+\.", n) is not None,
     "The field part can itself contain dots: filterBy.attrs.transcriptType, "
     "filterBy.vep.Consequence, filter.src.SP."),
    ("<mafTrack>.<accession species>",
     lambda n: re.search(r"\.GC[AF]_\d+\.\d+$", n) is not None,
     "A maf species column that is a GenArk accession, so the species token "
     "itself contains a dot.  Any [^.]+ for the species is wrong."),
]

HGTA_SHAPES = [
    ("hgta_fil.v.<db>.<table>.<field>.<op>",
     lambda n: n.startswith("hgta_fil.v.") and n.count(".") == 5,
     "catalogued with .pat only; .cmp and .dd are also in use"),
    ("hgta_fs.check.<db>.<table>.<field>",
     lambda n: n.startswith("hgta_fs.check.") and n.count(".") == 4,
     "catalogued"),
    ("hgta_fs.linked.<db>.<table>",
     lambda n: n.startswith("hgta_fs.linked."),
     "not catalogued"),
    ("hgta_fil.linked.<db>.<table>",
     lambda n: n.startswith("hgta_fil.linked."),
     "not catalogued"),
]


# ---------------------------------------------------------------- text reports

def reportCheck(audit, out=sys.stdout):
    print("sessions read      %d" % audit.nSess, file=out)
    print("distinct names     %d" % len(audit.names), file=out)
    print("name instances     %d" % sum(v[1] for v in audit.names.values()), file=out)
    print("track vocabulary   %d  (bare names whose values are all visibilities)"
          % len(audit.trackNames), file=out)
    print(file=out)
    for bucket, n in audit.counts().items():
        print("%-16s %8d" % (bucket, n), file=out)
    print(file=out)
    print("catalog patterns   literals %d  families %d  bare wildcards %d"
          % (len(audit.literals), len(audit.families), len(audit.wildcards)), file=out)
    print("bare wildcards     %s" % ", ".join(v for v, _ in audit.wildcards), file=out)


def reportRows(audit, bucket, out=sys.stdout, limit=None):
    rows = audit.sorted_(bucket)
    if limit:
        rows = rows[:limit]
    for nSess, total, name, values in rows:
        print("%6d\t%7d\t%s\t%s" % (nSess, total, name, " | ".join(values)), file=out)
    print("# %d names in bucket %s" % (len(audit.buckets[bucket]), bucket), file=out)


def reportWildcard(audit, out=sys.stdout, limit=60):
    for suffix, n in audit.wildcardSuffix.most_common(limit):
        print("%6d\t%s" % (n, suffix), file=out)
    print("# %d distinct suffixes matched only by a catch-all"
          % len(audit.wildcardSuffix), file=out)


def reportLeaks(audit, out=sys.stdout):
    seen, unseen = audit.leakReport()
    print("#37923 predicted %d leaks from reading the source." % len(audit.catLeaks),
          file=out)
    print("%d of them are in saved sessions right now:" % len(seen), file=out)
    for nSess, name in seen:
        print("  %6d  %s" % (nSess, name), file=out)
    print(file=out)
    print("not seen in any session (%d).  Absence is not safety, only that "
          "nobody saved a session after using them:" % len(unseen), file=out)
    print("  %s" % ", ".join(unseen), file=out)


def reportFindings(audit, text, out=sys.stdout):
    def line(label, stats):
        print("  %-38s names %-7d sessions %-8d busiest %s (%d)"
              % (label, stats["names"], stats["sessions"],
                 stats["busiest"], stats["busiestSess"]), file=out)

    print("== four ways to scope a variable to an assembly ==", file=out)
    for shape, src, example in DB_SCOPES:
        print("  %-24s %-44s %s in %d sessions"
              % (shape, src, example, audit.sess(example)), file=out)
    print(file=out)
    print("== the <var>_<db> family in use ==", file=out)
    for var, _note in DB_SUFFIX_VARS:
        line(var + "_<db>", dbSuffixFamily(audit, var))
    print(file=out)
    print("== global variables neither catalog covers ==", file=out)
    for title, src, vars_ in GLOBAL_GROUPS:
        present = [(audit.sess(v), v) for v in vars_ if v in audit.names]
        present.sort(reverse=True)
        print("  %s  (%s)" % (title, src), file=out)
        print("    %s" % ", ".join("%s(%d)" % (v, c) for c, v in present), file=out)
    print(file=out)
    print("== track variables the #37838 catalog lacks ==", file=out)
    for label, pred, _note in MISSING_TRACK_VARS:
        line(label, nameStats(audit, pred))
    fields, catalogued = labelFields(audit)
    print("  label.<field>: %d distinct fields observed, %d of them catalogued"
          % (len(fields), len(set(fields) & catalogued)), file=out)
    print(file=out)
    print("== hgTables shapes ==", file=out)
    for label, pred, note in HGTA_SHAPES:
        stats = nameStats(audit, pred)
        print("  %-42s %6d names  (%s)" % (label, stats["names"], note), file=out)
    print(file=out)
    print("== the nested cart inside position.<db> ==", file=out)
    inner, forms, dbs = innerPositionCarts(audit, text)
    print("  %d assemblies, value forms: %s"
          % (len(dbs), dict(forms)), file=out)
    for var, n in inner.most_common(12):
        print("    %-26s %d" % (var, n), file=out)
    print(file=out)
    print("== nothing rejects a name ==", file=out)
    for title, _note, pred in NOISE_CLASSES:
        stats = nameStats(audit, pred)
        print("  %-32s %5d names, busiest %s (%d sessions)"
              % (title, stats["names"], stats["busiest"], stats["busiestSess"]),
              file=out)
    present = [(audit.sess(t), t) for t in TYPOS if t in audit.names]
    print("  %-32s %5d of the %d known typos are in saved sessions: %s"
          % ("Typos, frozen forever", len(present), len(TYPOS),
             ", ".join(t for _c, t in sorted(present, reverse=True))), file=out)


# ---------------------------------------------------------------- json

def asJson(audit, text):
    inner, forms, dbs = innerPositionCarts(audit, text)
    fields, catalogued = labelFields(audit)
    seen, unseen = audit.leakReport()
    return {
        "ticket": "#37838, #37923",
        "what": "audit of the two cart catalogs against every named session",
        "corpus": {"sessions": audit.nSess,
                   "distinctNames": len(audit.names),
                   "nameInstances": sum(v[1] for v in audit.names.values())},
        "coverage": audit.counts(),
        "bareWildcards": [v for v, _ in audit.wildcards],
        "dbScopes": [{"shape": s, "src": c, "example": e, "sessions": audit.sess(e)}
                     for s, c, e in DB_SCOPES],
        "dbSuffixFamily": {v: dbSuffixFamily(audit, v) for v, _n in DB_SUFFIX_VARS},
        "globalGroups": [{"title": t, "src": s,
                          "vars": {v: audit.sess(v) for v in vs if v in audit.names}}
                         for t, s, vs in GLOBAL_GROUPS],
        "missingTrackVars": [{"shape": l, "note": n, "stats": nameStats(audit, p)}
                             for l, p, n in MISSING_TRACK_VARS],
        "labelFields": {"observed": len(fields), "catalogued": len(catalogued),
                        "uncatalogued": sorted(set(fields) - catalogued)},
        "hgtaShapes": [{"shape": l, "status": n, "names": nameStats(audit, p)["names"]}
                       for l, p, n in HGTA_SHAPES],
        "positionInnerCart": {"assemblies": len(dbs), "valueForms": dict(forms),
                              "vars": dict(inner)},
        "noise": [{"title": t, "note": n, "stats": nameStats(audit, p)}
                  for t, n, p in NOISE_CLASSES],
        "leaks": {"predicted": len(audit.catLeaks),
                  "observed": {n: c for c, n in seen},
                  "notObserved": unseen},
        "unknown": {n: audit.names[n][0] for n in audit.buckets["unknown"]},
    }


# ---------------------------------------------------------------- html

CSS = """
body { font: 14px/1.5 -apple-system, "Segoe UI", Helvetica, Arial, sans-serif;
       margin: 0; color: #12191f; background: #fff; }
header { background: #14385c; color: #fff; padding: 18px 28px; }
header h1 { margin: 0 0 4px; font-size: 20px; font-weight: 600; }
header p { margin: 0; font-size: 13px; color: #c5d6e8; max-width: 90ch; }
main { max-width: 1080px; margin: 0 auto; padding: 22px 28px 60px; }
h2 { font-size: 17px; margin: 34px 0 6px; padding-bottom: 5px;
     border-bottom: 2px solid #14385c; }
h3 { font-size: 15px; margin: 22px 0 4px; color: #14385c; }
p, li { max-width: 88ch; }
p.what { margin: 4px 0 10px; color: #4a5764; font-size: 13px; max-width: 88ch; }
table { border-collapse: collapse; width: 100%; margin: 6px 0 14px; font-size: 13px; }
th { text-align: left; background: #eef2f6; padding: 5px 8px;
     border-bottom: 1px solid #c9d3dd; font-weight: 600; }
td { padding: 5px 8px; border-bottom: 1px solid #eceff2; vertical-align: top; }
tr:hover td { background: #f7fafd; }
code, .n { font-family: ui-monospace, Menlo, Consolas, monospace; font-size: 12.5px; }
.n { font-weight: 600; color: #0b3d62; white-space: nowrap; }
.v { color: #7a3ba8; font-family: ui-monospace, Menlo, Consolas, monospace;
     font-size: 12.5px; word-break: break-all; }
.src { color: #6b7885; font-size: 11.5px; }
.note { color: #4a5764; font-size: 12.5px; }
.num { text-align: right; white-space: nowrap; font-variant-numeric: tabular-nums; }
.legend { background: #f7fafd; border: 1px solid #dde5ec; padding: 12px 14px;
          font-size: 13px; margin: 12px 0 4px; }
.finding { background: #fff8e1; border-left: 4px solid #e0a800; padding: 10px 14px;
           margin: 12px 0; font-size: 13px; }
.bug { background: #fdf1f1; border-left: 4px solid #c05050; padding: 10px 14px;
       margin: 12px 0; font-size: 13px; }
.ok { background: #eef7f1; border-left: 4px solid #3f8f5f; padding: 10px 14px;
      margin: 12px 0; font-size: 13px; }
"""


def esc(s):
    return html.escape(str(s))


def n(x):
    return "{:,}".format(x)


def renderHtml(audit, text):
    o = []
    add = o.append
    fields, catalogued = labelFields(audit)
    inner, forms, dbs = innerPositionCarts(audit, text)
    seen, unseen = audit.leakReport()
    counts = audit.counts()

    add("<title>What the saved sessions say the cart grammar is missing "
        "(#37838, #37923)</title>")
    add("<style>%s</style>" % CSS)
    add("<header><h1>What the saved sessions say the cart and URL grammar is "
        "missing</h1><p>Refs #37838 and #37923. Generated by "
        "hg/utils/sessionCartAudit/sessionCartAudit.py, not hand-edited. Every "
        "named session read back and matched against both catalogs.</p></header><main>")

    # method
    add("<h2>What was measured</h2>")
    add("<p class='what'>All %s named sessions in <code>namedSessionDb</code>, "
        "giving %s variable-name instances and <b>%s distinct names</b>. Each "
        "name was matched right to left against the longest suffix either "
        "catalog knows, at a <code>.</code> or <code>_</code> boundary. The bare "
        "wildcard entries in the #37838 catalog (%s) match any token at all, so "
        "they are scored separately; letting them match would absorb every "
        "unrecognised name and prove nothing.</p>"
        % (n(audit.nSess), n(sum(v[1] for v in audit.names.values())),
           n(len(audit.names)),
           ", ".join("<code>%s</code>" % esc(v) for v, _ in audit.wildcards)))

    add("<table><tr><th>bucket</th><th class='num'>distinct names</th>"
        "<th>meaning</th></tr>")
    for key, meaning in [
            ("trackVis", "a bare track name whose value is hide/dense/squish/pack/full"),
            ("trackKnown", "matched a real entry in the #37838 catalog"),
            ("wildcardOnly", "matched only a catch-all pattern, so effectively uncatalogued"),
            ("urlKnown", "matched an entry in the #37923 catalog"),
            ("unknown", "matched nothing in either catalog")]:
        add("<tr><td>%s</td><td class='num'>%s</td><td class='note'>%s</td></tr>"
            % (key, n(counts[key]), meaning))
    add("</table>")

    # 1 db scopes
    add("<h2>1. Four ways to scope a variable to an assembly, and the spec names none</h2>")
    add("<p class='what'>Both catalogs are organised around the track. Neither "
        "has a level for &quot;this variable belongs to a database&quot;. The "
        "tree has four separate conventions for it, all in live use.</p>")
    add("<table><tr><th>shape</th><th>defined at</th><th class='num'>sessions</th></tr>")
    for shape, src, example in DB_SCOPES:
        add("<tr><td class='n'>%s</td><td class='src'>%s</td><td class='num'>%s "
            "<span class='src'>(%s)</span></td></tr>"
            % (esc(shape), esc(src), n(audit.sess(example)), esc(example)))
    add("</table>")
    add("<div class='finding'><b>Why this matters for the schema.</b> "
        "<code>&lt;var&gt;_&lt;db&gt;</code> and the legacy track form "
        "<code>&lt;track&gt;_&lt;var&gt;</code> are the same string shape with "
        "the parts in opposite order. <code>revCmplDisp_hg38</code> cannot be "
        "told from a track called <code>revCmplDisp</code> with a variable "
        "called <code>hg38</code> by any amount of parsing. Only a list of which "
        "names are variables separates them, which is what these two tickets "
        "are for.</div>")
    add("<h3>The <code>&lt;var&gt;_&lt;db&gt;</code> family in use</h3>")
    add("<table><tr><th>variable</th><th class='num'>sessions</th>"
        "<th>what it holds</th></tr>")
    for var, note in DB_SUFFIX_VARS:
        st = dbSuffixFamily(audit, var)
        add("<tr><td class='n'>%s_&lt;db&gt;</td><td class='num'>%s "
            "<span class='src'>(%d assemblies)</span></td><td class='note'>%s</td></tr>"
            % (esc(var), n(st["sessions"]), st["names"], esc(note)))
    add("</table>")

    # 2 nested cart
    add("<h2>2. One cart variable holds an entire second cart</h2>")
    add("<p class='what'><code>cartSetDbPosition()</code> at hg/lib/cart.c:4078 "
        "calls <code>cartEncodeState()</code> and stores the whole result as the "
        "value of <code>position.&lt;db&gt;</code>, so a var=val blob is CGI-encoded "
        "and stuffed inside one value of the outer var=val blob. %s of the %s "
        "sessions have one, across %d assemblies. Raw, it looks like this:</p>"
        % (n(audit.sess("position.hg38")), n(audit.nSess), len(dbs)))
    add("<p class='v' style='background:#f7fafd;padding:8px 10px;"
        "border:1px solid #dde5ec'>position.hg38=lastVirtModeType%3Ddefault"
        "%26lastVirtModeExtraState%3D%26virtModeType%3Ddefault%26virtMode%3D0"
        "%26nonVirtPosition%3D%26position%3Dchr4%253A110617423%252D110623077</p>")
    if inner:
        add("<p class='what'>The inner cart has its own variable set, none of it "
            "catalogued:</p>")
        add("<table><tr><th>inner variable</th><th class='num'>occurrences</th></tr>")
        for var, c in inner.most_common():
            if c < 5:
                continue
            add("<tr><td class='n'>%s</td><td class='num'>%s</td></tr>" % (esc(var), n(c)))
        add("</table>")
    add("<p class='what'>Value forms seen: %s. Some are in an older bare form "
        "(just the position, no inner cart), and a handful are in a third form "
        "again, a plus-separated tuple such as "
        "<code>virt:69139406-69143353+chr8:11438769-11619126+exonMostly+emGeneTable</code>. "
        "Three formats in one variable.</p>"
        % esc(", ".join("%s %s" % (k, n(v)) for k, v in forms.items())))
    add("<div class='bug'><b>Bug found on the way through.</b> hg/lib/cart.c:4063 "
        "writes <code>lastVirtModeExtra</code> into the fresh inner cart, but "
        "every reader in the tree wants <code>lastVirtModeExtraState</code> "
        "(hgTracks.c:10957, 11276, 11302; web.c:1130). The short name appears "
        "exactly once in the whole source, at that write.</div>")

    # 3 globals
    add("<h2>3. The global browser settings have no catalog at all</h2>")
    add("<p class='what'>#37838 covers variables scoped to a track. #37923 covers "
        "what may go on a URL. A large and heavily used group falls between them: "
        "cart variables that are neither. They are what the Configure page writes, "
        "and they are in nearly every session.</p>")
    add("<table><tr><th>group</th><th>variables</th><th class='num'>busiest</th></tr>")
    for title, src, vars_ in GLOBAL_GROUPS:
        present = sorted(((audit.sess(v), v) for v in vars_ if v in audit.names),
                         reverse=True)
        if not present:
            continue
        add("<tr><td><b>%s</b><br><span class='src'>%s</span></td>"
            "<td class='note'>%s</td><td class='num'><code>%s</code><br>"
            "<span class='src'>in %s sessions</span></td></tr>"
            % (esc(title), esc(src),
               ", ".join("<code>%s</code>" % esc(v) for _c, v in present),
               esc(present[0][1]), n(present[0][0])))
    add("</table>")
    add("<div class='finding'><b>Suggested shape.</b> These divide three ways: "
        "settings the user chose and expects to keep, view state the browser "
        "recomputes every request (<code>winStart</code>, <code>insideX</code>, "
        "<code>rulerClickHeight</code>), and one-request form scratch that should "
        "never have been saved (the track-search group is a form's field set). "
        "Only the first belongs in a shared session, and nothing in the current "
        "format can tell them apart.</div>")

    # 4 missing track vars
    add("<h2>4. Track-scoped variables the #37838 catalog does not have</h2>")
    add("<table><tr><th>variable</th><th class='num'>distinct names</th>"
        "<th class='num'>busiest</th><th>where it comes from</th></tr>")
    for label, pred, note in MISSING_TRACK_VARS:
        st = nameStats(audit, pred)
        add("<tr><td class='n'>%s</td><td class='num'>%s</td>"
            "<td class='num'><code>%s</code><br><span class='src'>%s sessions</span></td>"
            "<td class='note'>%s</td></tr>"
            % (esc(label), n(st["names"]), esc(st["busiest"] or ""),
               n(st["busiestSess"]), esc(note)))
    add("</table>")
    extra = sorted(set(fields) - catalogued, key=lambda f: -fields[f])
    add("<div class='finding'><b><code>label.&lt;field&gt;</code> cannot be "
        "enumerated.</b> The catalog lists %d fixed names. The sessions contain "
        "<b>%d distinct fields</b>, %d of them uncatalogued, from "
        "<code>%s</code> in %s sessions down to a long tail of one. It has to be "
        "a family, not a list.</div>"
        % (len(catalogued), len(fields), len(extra),
           esc(extra[0]) if extra else "", n(fields[extra[0]]) if extra else 0))
    add("<div class='finding'><b>The JavaScript side was never harvested.</b> "
        "The #37838 notes recorded JS-only variables as something to come back "
        "to. <code>childShowCfg</code> is written under %s different track names "
        "and appears wherever the UI has views, yet it exists nowhere in the C. "
        "<code>hlColor</code> (hui.js:1566, hgTracks.js:2260) is the same class. "
        "A harvester that reads only C will keep missing all of it.</div>"
        % n(nameStats(audit, lambda x: x.endswith(".childShowCfg"))["names"]))

    # 5 separators
    add("<h2>5. Name components contain the separators, so the parse must be "
        "right-anchored</h2>")
    add("<p class='what'>The catalog's own notes warn that "
        "<code>&lt;track&gt;.&lt;var&gt;</code> is not a safe two-part parse "
        "because of composites. The sessions give a second, independent reason: "
        "the components themselves contain dots, spaces and slashes.</p>")
    add("<table><tr><th>example</th><th>problem</th></tr>")
    for example, problem in [
        ("vgp577way.GCF_020740605.2",
         "A maf species column that is a GenArk accession, so the species token "
         "contains a dot. %s distinct names of this shape."
         % n(nameStats(audit, lambda x: re.search(r"\.GC[AF]_\d+\.\d+$", x)
                       is not None)["names"])),
        ("strainName44way.CoV_BtRs-BetaCoV/YN2018D", "Species token with a slash."),
        ("hub_220_ts_regions .yLineOnOff",
         "Track name with a trailing space. %d names contain a space."
         % nameStats(audit, lambda x: " " in x)["names"]),
        ("hub_220_TS_3Seq_rep4_uniq_( )_imgOrd",
         "Parentheses and a space, then the legacy underscore separator."),
        ("(the empty string)",
         "One name is empty, in %d sessions. %d more contain non-ASCII bytes."
         % (audit.sess(""),
            nameStats(audit, lambda x: any(ord(ch) > 126 for ch in x))["names"])),
    ]:
        add("<tr><td class='n'>%s</td><td class='note'>%s</td></tr>"
            % (esc(example), problem))
    add("</table>")
    add("<div class='finding'><b>Rule this implies.</b> Match right-anchored, "
        "against a known vocabulary of variable names, never left-anchored at "
        "the first separator. That is only possible once the vocabulary is "
        "complete, which is the argument for finishing both catalogs before "
        "publishing a schema rather than after.</div>")

    # 6 hgTables
    add("<h2>6. The hgTables namespace is deeper than catalogued</h2>")
    add("<table><tr><th>shape</th><th class='num'>names</th><th>status</th></tr>")
    for label, pred, status in HGTA_SHAPES:
        add("<tr><td class='n'>%s</td><td class='num'>%s</td><td class='note'>%s</td></tr>"
            % (esc(label), n(nameStats(audit, pred)["names"]), esc(status)))
    add("</table>")

    # 7 noise
    add("<h2>7. Nothing rejects a variable name, and the sessions prove it</h2>")
    add("<p class='what'>Any name in a query string becomes a cart variable and "
        "stays. The saved sessions are a record of everything ever typed at the "
        "browser. This is the strongest argument in the data for validating "
        "names against a registry on the way in.</p>")
    add("<table><tr><th>class</th><th class='num'>distinct</th><th>examples</th></tr>")
    for title, note, pred in NOISE_CLASSES:
        hits = sorted(((audit.sess(x), x) for x in audit.names if pred(x)), reverse=True)
        if not hits:
            continue
        add("<tr><td><b>%s</b><br><span class='src'>%s</span></td>"
            "<td class='num'>%d</td><td class='note'>%s</td></tr>"
            % (esc(title), esc(note), len(hits),
               ", ".join("<code>%s</code> (%s)" % (esc(x), n(c)) for c, x in hits[:8])))
    present = sorted(((audit.sess(t), t) for t in TYPOS if t in audit.names), reverse=True)
    add("<tr><td><b>Typos, frozen forever</b><br><span class='src'>a slip in a "
        "URL, kept for years</span></td><td class='num'>%d</td>"
        "<td class='note'>%s</td></tr>"
        % (len(present), ", ".join("<code>%s</code>" % esc(t) for _c, t in present)))
    add("</table>")
    add("<div class='finding'><b>Direct bearing on #37838 phase 4.</b> The "
        "JSON-in path is a new front door. If it validates names against the "
        "#37923 registry and refuses the rest, it is the first entry point the "
        "browser has ever had that cannot accumulate this.</div>")

    # 8 leaks
    add("<h2>8. The leak audit checks out against real data</h2>")
    add("<div class='ok'><b>%d of the %d leaks predicted by "
        "<code>--reconcile</code> are sitting in saved sessions right now.</b> "
        "The prediction came from reading the source; this is independent "
        "confirmation from the data.</div>" % (len(seen), len(audit.catLeaks)))
    add("<table><tr><th>leaked parameter</th><th class='num'>sessions</th></tr>")
    for c, name in seen:
        add("<tr><td class='n'>%s</td><td class='num'>%s</td></tr>" % (esc(name), n(c)))
    add("</table>")
    add("<p class='what'>The %d not seen in any session: %s. Absence is not "
        "evidence they are safe, only that nobody saved a session after using "
        "them.</p>" % (len(unseen), ", ".join("<code>%s</code>" % esc(x) for x in unseen)))

    add("<p class='src' style='margin-top:30px'>Source: %s named sessions. "
        "Counts are distinct variable names unless labelled otherwise; "
        "&quot;sessions&quot; means the number of saved sessions containing that "
        "name.</p>" % n(audit.nSess))
    add("</main>")
    return "\n".join(o)


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--central", default=None,
                    help="hgcentral database to read (default from ~/.hg.conf)")
    ap.add_argument("--dump", help="read a saved contents dump instead of the db")
    ap.add_argument("--save-dump", dest="saveDump",
                    help="write the raw dump for later runs")
    ap.add_argument("--check", action="store_true", help="coverage counts")
    ap.add_argument("--unknown", action="store_true",
                    help="names neither catalog knows, one per line")
    ap.add_argument("--wildcard", action="store_true",
                    help="suffixes matched only by a bare wildcard")
    ap.add_argument("--leaks", action="store_true",
                    help="#37923 leak predictions against the data")
    ap.add_argument("--findings", action="store_true",
                    help="the curated findings, with counts recomputed")
    ap.add_argument("--json")
    ap.add_argument("--html")
    args = ap.parse_args()

    if args.dump:
        text = open(args.dump, encoding="utf8", errors="replace").read()
    else:
        text = dumpSessions(args.central or defaultCentral())
        if args.saveDump:
            with open(args.saveDump, "w") as f:
                f.write(text)
            print("wrote %s" % args.saveDump, file=sys.stderr)

    names, nSess = extract(text)
    audit = Audit(names, nSess)

    did = False
    if args.check:
        reportCheck(audit); did = True
    if args.unknown:
        reportRows(audit, "unknown"); did = True
    if args.wildcard:
        reportWildcard(audit); did = True
    if args.leaks:
        reportLeaks(audit); did = True
    if args.findings:
        reportFindings(audit, text); did = True
    if args.json:
        with open(args.json, "w") as f:
            json.dump(asJson(audit, text), f, indent=1)
        print("wrote %s" % args.json); did = True
    if args.html:
        with open(args.html, "w") as f:
            f.write(renderHtml(audit, text))
        print("wrote %s" % args.html); did = True

    if not did:
        reportCheck(audit)
    return 0


if __name__ == "__main__":
    sys.exit(main())
