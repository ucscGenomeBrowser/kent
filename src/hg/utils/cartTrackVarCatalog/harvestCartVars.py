#!/usr/bin/env python3
"""harvestCartVars.py - find track-scoped cart variables in the kent tree.

Refs #37838.  This is the mechanical half of the cart variable inventory: it
scans the source for the two ways a track-scoped cart name gets built and
reports every suffix it finds, attributed to the function that reads or writes
it.  cartTrackVarCatalog.py (next to this file) is the curated half.  Run this
first when the tree has moved, then reconcile what falls out against the
catalog.

The two signals:

  1. cart*ClosestToHome(cart, tdb, parentLevel, "suffix")
     The suffix argument is track-scoped by construction, so the 4th argument
     of any such call is a cart variable name.

  2. safef(buf, sizeof buf, "%s.%s", track, SUFFIX)
     and the "%s.%s.%s" and "%s.<literal>" variants.  Note the suffix is the
     SECOND vararg, not the first: the first one is the track name.

Macro identifiers are resolved against every #define in the scanned trees plus
inc/, lib/ and hg/inc/, chased up to five levels deep so that things like
GRAY_LEVEL_SCORE_MIN -> SCORE_MIN -> "scoreMin" come out as strings.

What it cannot resolve it reports rather than drops:

  {ident}    an identifier with no #define found, usually a local variable
             holding a name computed at run time
  EXPR:...   a computed expression, e.g. a ternary

Those are signal, not noise.  They mark the places where the name is built at
run time, which is exactly where the hierarchy nests one level deeper:
filter.<field>, decorator.<name>.<var>, <track>.<species>.

Output needs curation.  The scan cannot tell a cart variable from a table
name or an SQL fragment, so expect to throw away things like _gold by hand.

One class of that is recognised here rather than by hand: a filename.  Code
that builds "%s.tmp" from a filename looks exactly like code that builds
"%s.heightPer" from a track name, so every such site used to arrive as a name
somebody had to write down as not-a-cart-variable, 15 of them at the last
count and a new one every few weeks.  fileNameLike() below answers the
question instead, by asking whether the trailing component is a file
extension.  The records still carry the name, because a harvest that hides
what it saw cannot be checked; it is the catalog's --reconcile that stops
asking a person about them, and --filenames lists exactly what the rule
claims.

Usage:
    harvestCartVars.py --by-func            # grouped by function, for reading
    harvestCartVars.py --by-var             # grouped by variable, with sites
    harvestCartVars.py --filenames          # what the filename rule claims
    harvestCartVars.py --json recs.json     # raw records
    harvestCartVars.py --dirs hg/hgc,hg/hgTables --by-func
"""

import argparse
import collections
import json
import os
import re
import sys

# The tree to scan.  KENT_SRC lets a nightly run point at a pristine
# checkout instead of somebody's working tree, where a stray .c file or a
# half-finished edit would show up as a finding.
ROOT = os.environ.get("KENT_SRC") or os.path.expanduser("~/kent/src")

# Everything that draws or configures a track.  hgc and hgTables are in the
# default list because they read and write per-track vars too, which is easy
# to forget.
DEFAULT_DIRS = "hg/lib,hg/hgTracks,hg/hgTrackUi,hg/cgilib,hg/hgc,hg/hgTables"

# Extra trees mined for #define values only, not scanned for call sites.
MACRO_DIRS = ["inc", "lib", "hg/inc"]

# File extensions that mark a harvested name as a filename rather than a cart
# variable.  Deliberately only the ones the tree builds today, plus tbi beside
# bai: every entry here is a name nobody has to classify again, so a guess adds
# a way to lose a real cart variable silently and buys nothing.  Adding one is
# a decision, and --filenames is how to check what it costs.
FILE_SUFFIXES = frozenset([
    "bai", "bb", "cgm", "eps", "err", "html", "ids", "log", "pdf", "png",
    "ps", "tbi", "tmp", "txt", "wig",
])


# How far after the safef to look for the accessor that consumes the buffer.
# Small on purpose: the read is normally the next line, and a buffer reused
# later in the function for something else must not excuse an unrelated name.
HGCONF_WINDOW = 6


def hgConfRead(txt, pos, dest, lineno):
    """Is the buffer this call just filled read back as an hg.conf name?

    jksql.c:1325 builds "<profile>.excludeDbs" with safef from a failover
    profile name and reads it with cfgOption on the very next line.  That is
    an hg.conf setting, not a cart variable: it belongs to hgConfCatalog's
    database-profile suffix family, and hgConfCatalog cannot find it either,
    because the name never appears as a literal.

    Nothing about the shape distinguishes the two.  What distinguishes them is
    which accessor consumes the buffer, so that is what this looks for: a
    cfg* call taking the same identifier within the next few lines.  dest has
    to be a plain identifier; anything else and we do not know what was
    filled in.
    """
    if not re.fullmatch(r'[A-Za-z_]\w*', dest.strip()):
        return False
    rx = re.compile(r'\bcfg[A-Za-z0-9]*\s*\(\s*%s\s*[,)]'
                    % re.escape(dest.strip()))
    end = pos
    for _ in range(HGCONF_WINDOW):
        nl = txt.find("\n", end)
        if nl < 0:
            break
        end = nl + 1
    return rx.search(txt[pos:end]) is not None


def fileNameLike(var):
    """Is this harvested name the tail of a filename rather than a cart name?

    The test is on the trailing dot-separated component, after the leading
    separator the harvester may or may not have captured, so ".tmp",
    "_ss.ps" and ".link.bb" all answer yes through the same rule.

    Two cleverer tests were tried and rejected.  The destination buffer's
    declaration does not decide it: psName and tmpName are char[PATH_LEN] but
    the .bai and .link.bb sites format into a plain buf and buffer.  Nor does
    the argument being formatted: it is a filename at some sites, a url at
    others and a table name at a third set, with no shared spelling.  The
    extension is the only part that is actually about the name, which is what
    this rule asks about, and it is the only part a reader can check.
    """
    name = var.lstrip("._")
    return name.rsplit(".", 1)[-1].lower() in FILE_SUFFIXES


# ---------------------------------------------------------------------------
# macro table
# ---------------------------------------------------------------------------

def build_macros(dirs):
    """(name -> literal, names defined inconsistently) over the scanned dirs.

    The second return value exists because a pooled table gives a name that two
    files define differently whichever value the directory listing reached
    first, so the answer changes between two checkouts of the same commit.
    Those names resolve to {NAME} unless the file being scanned defines them
    itself, the same call the per-file char * constants make.
    """
    macro = {}
    conflict = set()
    chains = []
    def_re = re.compile(
        r'^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s+'
        r'("(?:[^"\\]|\\.)*")\s*(?:/[/*].*)?$')
    chain_re = re.compile(
        r'^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s+'
        r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:/[/*].*)?$')
    for d in list(dirs) + MACRO_DIRS:
        p = os.path.join(ROOT, d)
        if not os.path.isdir(p):
            continue
        for fn in os.listdir(p):
            if not fn.endswith((".h", ".c")):
                continue
            for line in open(os.path.join(p, fn), errors="replace"):
                m = def_re.match(line)
                if m:
                    name, val = m.group(1), m.group(2)[1:-1]
                    if name in macro and macro[name] != val:
                        conflict.add(name)
                    macro.setdefault(name, val)
                    continue
                m = chain_re.match(line)
                if m:
                    chains.append((m.group(1), m.group(2)))
    for _ in range(5):
        for a, b in chains:
            if a not in macro and b in macro:
                macro[a] = macro[b]
                if b in conflict:
                    conflict.add(a)            # alias of an ambiguous name
    return macro, conflict


# #define NAME "literal" in the file being scanned.  Its own definition is the
# one that file means, whatever the rest of the tree says.
LOCAL_DEFINE_RE = re.compile(
    r'^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+'
    r'("(?:[^"\\]|\\.)*")[ \t]*(?:/[/*].*)?$', re.M)


def local_defines(text):
    # CONST_RE next door captures inside the quotes, so strip them here to
    # match: localconst holds bare values.
    return {m.group(1): m.group(2)[1:-1]
            for m in LOCAL_DEFINE_RE.finditer(text)}


# ---------------------------------------------------------------------------
# C parsing, such as it is
# ---------------------------------------------------------------------------

CONST_RE = re.compile(
    r'(?:static\s+)?(?:const\s+)?char\s*\*\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*'
    r'"((?:[^"\\]|\\.)*)"')

# Kent style puts function bodies at column 0, so a bare "safef(...)" at
# column 0 looks like a function definition.  Requiring a return type plus
# whitespace (or a star) before the name is what keeps that from matching.
FUNCDEF_RE = re.compile(
    r'^(?:static\s+)?(?:INLINE\s+)?(?:const\s+)?'
    r'(?:struct\s+[A-Za-z_]\w*|unsigned\s+\w+|[A-Za-z_]\w*)'
    r'(?:\s+\**\s*|\s*\*+\s*)'
    r'([A-Za-z_]\w*)\s*\([^;]*$')

CTH_RE = re.compile(r'\bcart\w*ClosestToHome\s*\(')
FMT_RE = re.compile(
    r'\b(?:safef|dyStringPrintf|sqlDyStringPrintf|printf|jsInlineF)\s*\(')


def split_args(s):
    """Split a C argument list at top-level commas, respecting strings."""
    out, depth, cur, i, instr = [], 0, "", 0, False
    while i < len(s):
        c = s[i]
        if instr:
            cur += c
            if c == "\\":
                cur += s[i+1:i+2]
                i += 2
                continue
            if c == '"':
                instr = False
            i += 1
            continue
        if c == '"':
            instr = True
            cur += c
            i += 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
            i += 1
            continue
        cur += c
        i += 1
    if cur.strip():
        out.append(cur.strip())
    return out


def resolve(arg, localconst, macro, conflict=None):
    """Turn an argument into the string it evaluates to, if we can.

    conflict is the set of names the tree defines inconsistently; without the
    file's own definition to go on, those stay {NAME} rather than taking
    whichever value was seen first.
    """
    arg = arg.strip()
    if re.fullmatch(r'"((?:[^"\\]|\\.)*)"', arg):
        return arg[1:-1]
    toks = re.findall(r'"(?:[^"\\]|\\.)*"|[A-Za-z_][A-Za-z0-9_]*', arg)
    plain = re.sub(r'"(?:[^"\\]|\\.)*"|[A-Za-z_][A-Za-z0-9_]*|\s+', '', arg)
    if plain == "" and toks:
        # nothing but literals and identifiers, i.e. C string concatenation
        vals = []
        for t in toks:
            if t.startswith('"'):
                vals.append(t[1:-1])
            elif t in localconst:
                vals.append(localconst[t])
            elif conflict and t in conflict:
                vals.append("{" + t + "}")
            elif t in macro:
                vals.append(macro[t])
            else:
                vals.append("{" + t + "}")
        return "".join(vals)
    return "EXPR:" + re.sub(r'\s+', ' ', arg)[:60]


def match_close(txt, i):
    """Index of the paren that closes the one at i."""
    depth, j, instr = 0, i, False
    while j < len(txt):
        c = txt[j]
        if instr:
            if c == "\\":
                j += 2
                continue
            if c == '"':
                instr = False
        elif c == '"':
            instr = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return j
        j += 1
    return len(txt) - 1


def enclosing_functions(lines):
    """Map 1-based line number to the name of the function containing it."""
    encl = [None] * (len(lines) + 2)
    cur = None
    for idx, line in enumerate(lines):
        if (line and not line[0].isspace()
                and not line.startswith(("#", "/", "*", "}", "{"))):
            m = FUNCDEF_RE.match(line)
            if m:
                cur = m.group(1)
        encl[idx + 1] = cur
    return encl


def scan_file(fp, rel, macro, conflict=None):
    """Return one record per track-scoped cart name found in one file."""
    txt = open(fp, errors="replace").read()
    # The file's own #defines join its char * constants: both are what this
    # file means, whatever the rest of the tree calls the same name.
    localconst = {m.group(1): m.group(2) for m in CONST_RE.finditer(txt)}
    localconst.update(local_defines(txt))
    encl = enclosing_functions(txt.split("\n"))

    starts = [0]
    for i, ch in enumerate(txt):
        if ch == "\n":
            starts.append(i + 1)

    def lineno(pos):
        lo, hi = 0, len(starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if starts[mid] <= pos:
                lo = mid
            else:
                hi = mid - 1
        return lo + 1

    recs = []

    for m in CTH_RE.finditer(txt):
        i = m.end() - 1
        args = split_args(txt[i+1:match_close(txt, i)])
        if len(args) >= 4:
            ln = lineno(m.start())
            recs.append(dict(var=resolve(args[3], localconst, macro,
                                         conflict),
                             file=rel, line=ln, func=encl[ln], how="cth"))

    for m in FMT_RE.finditer(txt):
        i = m.end() - 1
        args = split_args(txt[i+1:match_close(txt, i)])
        ln = lineno(m.start())
        fi = None
        for k, a in enumerate(args):
            if a.strip().startswith('"'):
                fi = k
                break
        if fi is None:
            continue
        fmt = resolve(args[fi], localconst, macro, conflict)
        rest = args[fi+1:]
        mm = re.match(r'^%s([._])(.*)$', fmt or "")
        if not mm:
            continue
        sep, tail = mm.group(1), mm.group(2)
        # An hg.conf name and a cart name are built the same way; only the
        # accessor that reads the buffer back tells them apart.
        conf = (len(args) > 0
                and hgConfRead(txt, match_close(txt, i) + 1, args[0], ln))
        def rec(var, how):
            r = dict(var=var, file=rel, line=ln, func=encl[ln], how=how)
            if conf:
                r["notCart"] = "hgConf"
            return r
        if "%" not in tail and tail:
            recs.append(rec(sep+tail, "fmtlit"))
        elif tail == "%s" and len(rest) >= 2:
            # "%s.%s", track, SUFFIX -> the suffix is the SECOND vararg
            v = resolve(rest[1], localconst, macro, conflict)
            if v and not v.startswith("EXPR:"):
                recs.append(rec(sep+v, "fmt"))
        elif tail == "%s.%s" and len(rest) >= 3:
            v1 = resolve(rest[1], localconst, macro, conflict)
            v2 = resolve(rest[2], localconst, macro, conflict)
            if not v1.startswith("EXPR:") and not v2.startswith("EXPR:"):
                recs.append(rec(sep+v1+"."+v2, "fmt3"))
    return recs


# ---------------------------------------------------------------------------
# entry point for the catalog next door
# ---------------------------------------------------------------------------

def harvest(dirs=None, quiet=False):
    """Scan the tree and return the raw records.

    cartTrackVarCatalog.py --reconcile imports this, so the scan has one
    definition rather than one here and a second one written out by hand.
    """
    dirs = dirs or [d.strip() for d in DEFAULT_DIRS.split(",") if d.strip()]
    macro, conflict = build_macros(dirs)

    files = []
    for d in dirs:
        p = os.path.join(ROOT, d)
        if not os.path.isdir(p):
            sys.exit("no such directory: %s" % p)
        for fn in sorted(os.listdir(p)):
            if fn.endswith(".c"):
                files.append(os.path.join(p, fn))

    records = []
    for fp in files:
        records.extend(scan_file(fp, os.path.relpath(fp, ROOT), macro,
                                 conflict))

    if not quiet:
        print("scanned %d files in %d dirs, %d macros, %d records"
              % (len(files), len(dirs), len(macro), len(records)),
              file=sys.stderr)
    return records


def hgConfNames(records):
    """The harvested names that are hg.conf settings, not cart variables.

    Keyed with the leading separator stripped, the way the catalog compares
    them.  See hgConfRead() for what makes the call.
    """
    return set(r["var"].lstrip("._") for r in records
               if r.get("notCart") == "hgConf" and not r["var"].startswith("EXPR:")
               and "{" not in r["var"])


def resolved(records):
    """name -> first file:line, for the names the scan resolved to a literal.

    An EXPR: or {ident} record marks a name built at run time, which is signal
    for a person reading the harvester output but cannot be compared against a
    catalog of literal names, so it is dropped here.
    """
    out = {}
    for r in sorted(records, key=lambda r: (r["file"], r["line"])):
        var = r["var"]
        if var.startswith("EXPR:") or "{" in var:
            continue
        out.setdefault(var, "%s:%d" % (r["file"], r["line"]))
    return out


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dirs", default=DEFAULT_DIRS,
                    help="comma-separated dirs under the kent src root to "
                         "scan (default: %s)" % DEFAULT_DIRS)
    ap.add_argument("--json", metavar="FILE", help="write raw records as JSON")
    ap.add_argument("--by-func", action="store_true",
                    help="group by file and function")
    ap.add_argument("--by-var", action="store_true",
                    help="group by variable, listing where each is used")
    ap.add_argument("--filenames", action="store_true",
                    help="list the harvested names the filename rule claims, "
                         "with the call site, so the rule can be audited")
    ap.add_argument("--hgconf", action="store_true",
                    help="list the harvested names that are hg.conf settings "
                         "rather than cart variables, with the call site")
    ap.add_argument("--keep-unresolved", action="store_true",
                    help="include {ident} and EXPR: entries in the groupings")
    args = ap.parse_args()

    dirs = [d.strip() for d in args.dirs.split(",") if d.strip()]
    records = harvest(dirs)

    def wanted(var):
        if args.keep_unresolved:
            return True
        return not var.startswith("EXPR:") and "{" not in var

    if args.json:
        with open(args.json, "w") as f:
            json.dump(records, f, indent=1)
        print("wrote %s" % args.json, file=sys.stderr)

    if args.filenames:
        hits = {}
        for r in sorted(records, key=lambda r: (r["file"], r["line"])):
            if wanted(r["var"]) and fileNameLike(r["var"]):
                hits.setdefault(r["var"].lstrip("._"),
                                "%s:%d" % (r["file"], r["line"]))
        print("%d harvested names read as filenames, not cart variables"
              % len(hits))
        for n in sorted(hits):
            print("    %-16s %s" % (n, hits[n]))

    if args.hgconf:
        hits = {}
        for r in sorted(records, key=lambda r: (r["file"], r["line"])):
            if wanted(r["var"]) and r.get("notCart") == "hgConf":
                hits.setdefault(r["var"].lstrip("._"),
                                "%s:%d" % (r["file"], r["line"]))
        print("%d harvested names read back with a cfg* accessor, so they are "
              "hg.conf\nsettings and not cart variables" % len(hits))
        for n in sorted(hits):
            print("    %-16s %s" % (n, hits[n]))

    if args.by_func:
        byfunc = collections.defaultdict(set)
        for r in records:
            if wanted(r["var"]):
                byfunc[(r["file"], r["func"])].add(r["var"])
        for k in sorted(byfunc):
            print("%s  %s()" % (k[0], k[1]))
            print("    " + ", ".join(sorted(byfunc[k])))

    if args.by_var:
        byvar = collections.defaultdict(set)
        for r in records:
            if wanted(r["var"]):
                byvar[r["var"]].add("%s:%d" % (r["file"], r["line"]))
        for k in sorted(byvar, key=str.lower):
            print("%-34s %d  %s"
                  % (k, len(byvar[k]), " ".join(sorted(byvar[k])[:4])))

    if not (args.json or args.by_func or args.by_var or args.filenames
            or args.hgconf):
        print("nothing to do; pass --by-func, --by-var, --filenames, "
              "--hgconf or --json", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
