#!/usr/bin/env python3
"""harvestCartFileVars.py - find cart variables that hold a server file name.

Refs #37623.  A handful of cart variables do not hold a setting, they hold the
name of a file the server made for the user: the custom track file, the user
regions file, the BLAT result files, the track collection hub.  A CGI reads one
back out of the cart and opens it.  Nothing about the read says the value is a
path, so the only way to know the set is to look for the value reaching a
file-system call.

That is what this scans for.  hg/lib/cart.c screens these variables on the way
in, against a hand-written list, and the point of the scan is to notice when
the tree grows a new one that nobody added to that list.

Two signals, both intraprocedural, because that is where the pattern lives:

  1. char *f = cartOptionalString(cart, NAME);  ...  mustOpen(f, "w")
     The value is bound to a local and a file call is made on that local
     somewhere in the same function body.  Every real case in the tree today
     has this shape.

  2. fileExists(cartUsualString(cart, NAME, ""))
     The read nested directly in the call.  None in the tree right now, but it
     costs nothing to keep and it is the obvious way to write the next one.

Macro identifiers are resolved against every #define in the scanned trees plus
inc/, lib/ and hg/inc/, chased five levels deep, so DUP_TRACKS_VAR comes out as
"dup_tracks".  A name built at run time cannot be resolved and is reported as
{ident}: hg/lib/customTrack.c builds "ctfile_" + db into a local, so its read
comes out as {ctFileVar}.  Those are signal, not noise - a computed name is
exactly the case the prefix half of the cart.c list exists for - and the
catalog next door is where each one gets tied to the family it belongs to.

What it cannot see:

  - a value that reaches a file call through a function argument.  hgBlat hands
    blatPslFile to showAliPlaces(), and only the fileExists() guard next to the
    read puts it in this scan at all.  Drop that guard and the flow disappears.
    --suspects is the wider net for this: it reports cart reads whose name reads
    like a file name, whether or not a sink was found.
  - a value that reaches a file call in a different function via a static.
  - anything in a CGI that is not built.  gsid and gisaid are in the tree and in
    this scan; neither is in hg/makefile.  Cross-check before concluding that
    something found here is reachable.

Usage:
    harvestCartFileVars.py                  # summary counts
    harvestCartFileVars.py --flows          # every value-to-file-call flow
    harvestCartFileVars.py --names          # flat sorted name list
    harvestCartFileVars.py --suspects       # file-ish names, sink or not
    harvestCartFileVars.py --screen         # the list cart.c screens against
    harvestCartFileVars.py --json out.json
"""

import argparse
import collections
import json
import os
import re
import sys

# The tree to scan.  KENT_SRC lets a nightly run point at a pristine checkout
# instead of somebody's working tree, where a stray .c file or a half-finished
# edit would show up as a finding.
ROOT = os.environ.get("KENT_SRC") or os.path.expanduser("~/kent/src")

# Walked for call sites.  hg/lib is in here because most of the sinks are there
# (customTrack.c, dupTrack.c, trackHub.c) rather than in any one CGI.
SCAN_ROOTS = ["hg", "lib"]

# Not source we care about, and walking them is slow.
SKIP_DIRS = {"htdocs", "js", "tests", "expected", "input", "trackDb",
             "makeDb/doc", "CVS", ".git", "python", "lowelab"}

# Mined for #define values in addition to everything under SCAN_ROOTS.
MACRO_DIRS = ["inc", "hg/inc"]

# Where the screening list lives, and the four arrays in it.  The third holds
# the names that may legitimately be a remote URL instead of a file, which are
# screened with isServerUserFileOrUrl() rather than isServerUserFilePath().  The
# fourth holds the ones whose value is two file names and a trailing word rather
# than one file name; cart.c checks the two names.
CART_C = os.path.join("hg", "lib", "cart.c")
SCREEN_ARRAYS = ("fileNameCartVars", "fileNameCartVarPrefixes",
                 "urlOrFileNameCartVars", "fileNamePairCartVarPrefixes")


# ---------------------------------------------------------------------------
# macro table
# ---------------------------------------------------------------------------

def macro_files():
    """Every .c and .h worth reading a #define out of."""
    seen = set()
    for root in list(SCAN_ROOTS) + MACRO_DIRS:
        base = os.path.join(ROOT, root)
        for dirpath, dirnames, filenames in os.walk(base):
            rel = os.path.relpath(dirpath, ROOT)
            if any(part in SKIP_DIRS for part in rel.split(os.sep)):
                dirnames[:] = []
                continue
            for fn in filenames:
                if fn.endswith((".c", ".h")):
                    path = os.path.join(dirpath, fn)
                    if path not in seen:
                        seen.add(path)
                        yield path


def build_macros():
    """(name -> literal, names defined inconsistently) over the whole tree.

    Two passes, because the tree defines names in terms of other names:
        #define CT_FILE_VAR_PREFIX  "ctfile_"
        #define CT_FILE_VAR_HUB     CT_FILE_VAR_PREFIX "hub_"
    The concatenating form is resolved over several rounds until nothing new
    appears.

    A pooled table answers for the whole tree, so a name that two files define
    differently would otherwise get whichever value the filesystem walk reached
    first.  Those are reported as ambiguous and resolve to {NAME} unless the
    file being scanned defines them itself.
    """
    macro = {}
    conflict = set()
    chains = []
    lit_re = re.compile(
        r'^\s*#\s*define\s+([A-Za-z_]\w*)\s+'
        r'("(?:[^"\\]|\\.)*")\s*(?:/[/*].*)?$')
    cat_re = re.compile(
        r'^\s*#\s*define\s+([A-Za-z_]\w*)\s+'
        r'((?:(?:"(?:[^"\\]|\\.)*")|(?:[A-Za-z_]\w*))'
        r'(?:\s+(?:(?:"(?:[^"\\]|\\.)*")|(?:[A-Za-z_]\w*)))*)'
        r'\s*(?:/[/*].*)?$')
    piece_re = re.compile(r'"(?:[^"\\]|\\.)*"|[A-Za-z_]\w*')

    for fn in macro_files():
        for line in open(fn, errors="replace"):
            m = lit_re.match(line)
            if m:
                name, val = m.group(1), m.group(2)[1:-1]
                if name in macro and macro[name] != val:
                    conflict.add(name)
                macro.setdefault(name, val)
                continue
            m = cat_re.match(line)
            if m:
                chains.append((m.group(1), piece_re.findall(m.group(2))))

    for _ in range(5):
        for name, pieces in chains:
            if name in macro:
                continue
            out = ""
            for piece in pieces:
                if piece.startswith('"'):
                    out += piece[1:-1]
                elif piece in macro:
                    out += macro[piece]
                    if piece in conflict:
                        conflict.add(name)
                else:
                    out = None
                    break
            if out is not None:
                macro[name] = out
    return macro, conflict


LOCAL_DEFINE_RE = re.compile(
    r'^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+'
    r'("(?:[^"\\]|\\.)*")[ \t]*(?:/[/*].*)?$', re.M)

# static char *customFileVar = "near.customFile";  Not every name is a #define.
CONST_RE = re.compile(
    r'^[ \t]*(?:static[ \t]+)?(?:const[ \t]+)?char[ \t]*\*[ \t]*([A-Za-z_]\w*)'
    r'[ \t]*=[ \t]*("(?:[^"\\]|\\.)*")[ \t]*;', re.M)


def local_consts(text):
    """The file's own char * constants and #defines.

    Never pooled across files: the same identifier means different things in
    different programs, and one file's varName would otherwise answer for all
    of them.
    """
    out = {m.group(1): m.group(2)[1:-1] for m in CONST_RE.finditer(text)}
    out.update({m.group(1): m.group(2)[1:-1]
                for m in LOCAL_DEFINE_RE.finditer(text)})
    return out


def resolve(tok, macro, localconst=None, conflict=None):
    """Turn one C token into the name it stands for, or {ident} if unknown."""
    tok = (tok or "").strip()
    if not tok:
        return None
    if tok.startswith('"') and tok.endswith('"') and len(tok) >= 2:
        return tok[1:-1]
    if tok == "NULL":
        return None
    if localconst and tok in localconst:
        return localconst[tok]
    if conflict and tok in conflict:
        return "{%s}" % tok
    if tok in macro:
        return macro[tok]
    if re.match(r'^[A-Za-z_]\w*$', tok):
        return "{%s}" % tok
    return None


# ---------------------------------------------------------------------------
# scanning
# ---------------------------------------------------------------------------

# The cart reads that hand back a string the caller can open.  cartString and
# cartNonemptyString errAbort rather than return NULL, which changes nothing
# here: the value is still whatever the user put in the cart.
CART_READ = (r'cart(?:Optional|Usual|Nonempty|CgiUsual)?String')

# name = cartOptionalString(cart, NAME)
READ_RE = re.compile(
    r'\b([A-Za-z_]\w*)\s*=\s*(?:\(\s*char\s*\*\s*\)\s*)?'
    + CART_READ + r'\s*\(\s*\w+\s*,\s*([^,()]+?)\s*[,)]')

# The file-system calls.  A value reaching any of these is being treated as a
# path.  Kept explicit rather than pattern-matched on "Open", because names like
# sqlOpenConnection and udcFileOpen of a URL would both match a pattern and only
# one of them is about a path in the file system.
SINKS = [
    # libc
    "fopen", "open", "unlink", "remove", "rename", "chmod", "stat", "lstat",
    "mkdir", "rmdir", "truncate", "creat",
    # kent common.c / linefile.c
    "mustOpen", "mustOpenFd", "fileExists", "fileSize", "mustRemove",
    "lineFileOpen", "lineFileMayOpen", "lineFileTabixMayOpen",
    "lineFileUdcMayOpen", "netLineFileOpen", "netLineFileMayOpen",
    "readAllText", "mustReadAll", "readAndIgnore", "slurpFile",
    "udcFileOpen", "udcFileMayOpen", "udcFileSize",
    # kent file-format openers
    "bigBedFileOpen", "bbiFileOpen", "bigWigFileOpen", "twoBitOpen",
    "vcfTabixFileMayOpen", "bamOpen", "hicFileOpen", "dnaLoadOpen",
    "customPpNew", "customPpOpen", "customFactoryParse",
]
SINK_RE = re.compile(
    r'\b(%s)\s*\(\s*(?:\(\s*char\s*\*\s*\)\s*)?([A-Za-z_]\w*)\s*[,)]'
    % "|".join(SINKS))

# fileExists(cartUsualString(cart, NAME, ""))
NESTED_RE = re.compile(
    r'\b(%s)\s*\(\s*' % "|".join(SINKS)
    + CART_READ + r'\s*\(\s*\w+\s*,\s*([^,()]+?)\s*[,)]')

# A name that reads like a file name.  Only used for --suspects, never to fail
# a reconcile: the point of the dataflow scan is not to have to guess from a
# name, and half of these are settings whose value is a URL.
FILEISH_RE = re.compile(
    r'(?:^|[._-])(?:file|path)|(?:File|Path|Ps|BigBed|Bed|Psl|Fa)$'
    r'|ctfile|Ctfile|customComposite|QuickLift|quickLift', re.X)


def source_files():
    for root in SCAN_ROOTS:
        base = os.path.join(ROOT, root)
        for dirpath, dirnames, filenames in os.walk(base):
            rel = os.path.relpath(dirpath, ROOT)
            if any(part in SKIP_DIRS for part in rel.split(os.sep)):
                dirnames[:] = []
                continue
            for fn in sorted(filenames):
                if fn.endswith(".c"):
                    yield os.path.join(dirpath, fn)


def function_regions(lines):
    """Split a kent .c file into function bodies, as (start, end) line indexes.

    Kent house style puts a function's opening brace alone in column 0, so the
    braces at column 0 are the function boundaries.  This is crude - it cannot
    see a function that breaks the style, and a file-scope initializer brace
    would open a spurious region - but a spurious region only ever widens the
    window a local is looked for in, which costs a false positive to review
    rather than a missed flow.
    """
    starts = [i for i, l in enumerate(lines) if l.startswith("{")]
    for bi, start in enumerate(starts):
        end = starts[bi + 1] if bi + 1 < len(starts) else len(lines)
        yield start, end


def scan(macro, conflict):
    """Every cart-value-to-file-call flow in the tree.

    Returns (flows, suspects).  A flow is a dict with the resolved cart
    variable name, the file and line of the file call, the call, and the local
    the value was bound to.  A suspect is a cart read whose name reads like a
    file name, with no sink required.
    """
    flows = []
    suspects = collections.defaultdict(set)

    for path in source_files():
        try:
            text = open(path, errors="replace").read()
        except OSError:
            continue
        rel = os.path.relpath(path, ROOT)
        lines = text.split("\n")
        localconst = local_consts(text)

        def name_of(tok):
            return resolve(tok, macro, localconst, conflict)

        for i, line in enumerate(lines):
            # signal 1 half: record every read, for --suspects
            for m in READ_RE.finditer(line):
                name = name_of(m.group(2))
                if name and FILEISH_RE.search(name):
                    suspects[name].add("%s:%d" % (rel, i + 1))
            # signal 2: the read nested straight in the file call
            for m in NESTED_RE.finditer(line):
                name = name_of(m.group(2))
                if name:
                    flows.append(dict(name=name, file=rel, line=i + 1,
                                      sink=m.group(1), local="(nested)"))

        # signal 1: bind to a local, then a file call on that local
        for start, end in function_regions(lines):
            bound = {}
            for i in range(start, end):
                for m in READ_RE.finditer(lines[i]):
                    name = name_of(m.group(2))
                    if name:
                        bound[m.group(1)] = name
            if not bound:
                continue
            for i in range(start, end):
                for m in SINK_RE.finditer(lines[i]):
                    sink, local = m.group(1), m.group(2)
                    if local in bound:
                        flows.append(dict(name=bound[local], file=rel,
                                          line=i + 1, sink=sink, local=local))

    # A flow found by both signals is one flow.
    seen, uniq = set(), []
    for f in flows:
        k = (f["name"], f["file"], f["line"], f["sink"])
        if k not in seen:
            seen.add(k)
            uniq.append(f)
    uniq.sort(key=lambda f: (f["name"], f["file"], f["line"]))
    return uniq, suspects


# ---------------------------------------------------------------------------
# the screening list in cart.c
# ---------------------------------------------------------------------------

ARRAY_RE_TMPL = (r'\b%s\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;')


def read_screen(macro, conflict, root=None):
    """One set per array in SCREEN_ARRAYS that cart.c screens, or all None.

    This is the other half of the loop: the scan says what the tree treats as a
    file name, this says what cart.c is willing to check.  Parsed out of the
    source rather than kept as a second copy here, so the two cannot drift.
    """
    path = os.path.join(root or ROOT, CART_C)
    try:
        text = open(path, errors="replace").read()
    except OSError:
        return (None,) * len(SCREEN_ARRAYS)
    localconst = local_consts(text)
    out = []
    for array in SCREEN_ARRAYS:
        m = re.search(ARRAY_RE_TMPL % array, text, re.S)
        if not m:
            out.append(None)
            continue
        # Strip comments before splitting on commas, not after: every entry in
        # these arrays carries a trailing // comment and several of those
        # contain a comma, which would otherwise cut an entry in half.
        body = re.sub(r'/\*.*?\*/', " ", m.group(1), flags=re.S)
        body = re.sub(r'//[^\n]*', " ", body)
        names = set()
        for entry in body.split(","):
            entry = entry.strip()
            if not entry:
                continue
            # An entry may concatenate: customCompositeCartName "-"
            pieces = re.findall(r'"(?:[^"\\]|\\.)*"|[A-Za-z_]\w*', entry)
            val = ""
            for piece in pieces:
                got = resolve(piece, macro, localconst, conflict)
                if got is None or got.startswith("{"):
                    val = None
                    break
                val += got
            if val:
                names.add(val)
        out.append(names)
    return tuple(out)


def screened(name, names, prefixes, urlOrFile=None, pairPrefixes=None):
    """Does cart.c check this cart variable on the way in, by any of the four?"""
    if names is None or prefixes is None:
        return False
    if name in names or name in (urlOrFile or ()):
        return True
    return any(name.startswith(p)
               for p in list(prefixes) + list(pairPrefixes or ()))


# ---------------------------------------------------------------------------
# reporting
# ---------------------------------------------------------------------------

def harvest():
    macro, conflict = build_macros()
    flows, suspects = scan(macro, conflict)
    names, prefixes, urlOrFile, pairPrefixes = read_screen(macro, conflict)
    srt = lambda s: sorted(s) if s is not None else None
    return dict(flows=flows, suspects={k: sorted(v)
                                       for k, v in suspects.items()},
                screenNames=srt(names), screenPrefixes=srt(prefixes),
                screenUrlOrFile=srt(urlOrFile),
                screenPairPrefixes=srt(pairPrefixes))


def by_name(flows):
    out = collections.defaultdict(list)
    for f in flows:
        out[f["name"]].append(f)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--flows", action="store_true",
                    help="every value-to-file-call flow, grouped by cart name")
    ap.add_argument("--names", action="store_true",
                    help="flat sorted list of cart names with a flow")
    ap.add_argument("--suspects", action="store_true",
                    help="cart reads whose name reads like a file name")
    ap.add_argument("--screen", action="store_true",
                    help="the list hg/lib/cart.c screens against")
    ap.add_argument("--json", metavar="FILE", help="write the whole harvest")
    args = ap.parse_args()

    h = harvest()
    flows, groups = h["flows"], by_name(h["flows"])

    if args.json:
        with open(args.json, "w") as f:
            json.dump(h, f, indent=2, sort_keys=True)
        print("wrote %s" % args.json)
        return 0

    if args.screen:
        if h["screenNames"] is None:
            print("no fileNameCartVars[] in %s" % CART_C)
            return 1
        print("fileNameCartVars (%d)" % len(h["screenNames"]))
        for n in h["screenNames"]:
            print("    %s" % n)
        print("fileNameCartVarPrefixes (%d)" % len(h["screenPrefixes"]))
        for n in h["screenPrefixes"]:
            print("    %s" % n)
        print("urlOrFileNameCartVars (%d)" % len(h["screenUrlOrFile"] or []))
        for n in h["screenUrlOrFile"] or []:
            print("    %s" % n)
        print("fileNamePairCartVarPrefixes (%d)"
              % len(h["screenPairPrefixes"] or []))
        for n in h["screenPairPrefixes"] or []:
            print("    %s" % n)
        return 0

    if args.names:
        for n in sorted(groups):
            print(n)
        return 0

    if args.suspects:
        for n in sorted(h["suspects"]):
            mark = " (flow)" if n in groups else ""
            print("%s%s" % (n, mark))
            for site in h["suspects"][n]:
                print("    %s" % site)
        return 0

    if args.flows:
        names = set(h["screenNames"] or [])
        prefixes = set(h["screenPrefixes"] or [])
        urlOrFile = set(h["screenUrlOrFile"] or [])
        pairPrefixes = set(h["screenPairPrefixes"] or [])
        for n in sorted(groups):
            mark = ("screened" if screened(n, names, prefixes, urlOrFile,
                                           pairPrefixes)
                    else "NOT screened")
            print("%s  [%s]" % (n, mark))
            for f in groups[n]:
                print("    %s:%d  %s(%s)"
                      % (f["file"], f["line"], f["sink"], f["local"]))
        return 0

    print("cart variables with a value reaching a file call   %d" % len(groups))
    print("flows                                              %d" % len(flows))
    print("file-ish cart names (--suspects)                   %d"
          % len(h["suspects"]))
    if h["screenNames"] is None:
        print("cart.c screening list                              absent")
    else:
        print("cart.c screening list                              %d names, "
              "%d prefixes" % (len(h["screenNames"]), len(h["screenPrefixes"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
