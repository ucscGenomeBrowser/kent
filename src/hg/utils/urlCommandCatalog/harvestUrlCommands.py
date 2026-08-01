#!/usr/bin/env python3
"""harvestUrlCommands.py - find URL commands in the kent tree.

Refs #37923.  This is the mechanical half of the URL command inventory.  A URL
command is a CGI parameter that is NOT persisted cart state: it asks for an
action, or it is consumed and dropped.  Three separate mechanisms accept one,
and this script scans for all three so the curated catalog next door
(urlCommandCatalog.py) has something to be reconciled against.

The three signals:

  1. char *excludeVars[] = { ... }
     Every CGI declares one.  Its members are the CGI variables that cartNew()
     refuses to write back to the cart, so by construction they are one-shot.
     Members may be string literals or macro identifiers.

  2. cgiOptionalString("x") / cgiVarExists("x") / cgiUsualString("x", ...)
     A read straight from the CGI variables that bypasses the cart entirely.
     These are the invisible ones: nothing about the declaration says the
     parameter exists, and it never appears in excludeVars, so it cannot be
     found by reading the cart machinery.

  3. cartRemove(cart, "x") / cartRemovePrefix(cart, "x")
     A parameter that rides in through the cart and is deleted after use.
     Transient in effect, but indistinguishable from real cart state until you
     notice the removal.

Macro identifiers are resolved against every #define in the scanned trees plus
inc/, lib/ and hg/inc/, chased five levels deep, so hgHubDataText comes out as
"hubUrl" and CT_CUSTOM_TEXT_VAR as "hgt.customText".

What it cannot resolve it reports rather than drops, as {ident}.  Those mark
names built at run time and are signal, not noise.

Output needs curation.  The scan cannot tell a URL command from a form-button
name, a table name or an SQL fragment, and it cannot tell whether a CGI is
still built.  Cross-check against BROWSER_BINS in src/makefile before
concluding that anything found here is reachable.

Usage:
    harvestUrlCommands.py                   # summary counts
    harvestUrlCommands.py --exclude-vars    # mechanism 1, grouped by CGI
    harvestUrlCommands.py --cgi-reads       # mechanism 2, grouped by CGI
    harvestUrlCommands.py --cart-removes    # mechanism 3, grouped by CGI
    harvestUrlCommands.py --all             # every mechanism, grouped by CGI
    harvestUrlCommands.py --names           # flat sorted name list
    harvestUrlCommands.py --json out.json
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

# Walked for call sites.  hg/lib and hg/cgilib are in here because the cart
# machinery itself reads URL commands (hgsid, ignoreCookie, the session
# loaders), which a CGI-only scan would miss entirely.
SCAN_ROOTS = ["hg", "lib"]

# Not source we care about, and walking them is slow.
SKIP_DIRS = {"htdocs", "js", "tests", "expected", "input", "trackDb",
             "makeDb/doc", "CVS", ".git", "python", "lowelab"}

# Mined for #define values in addition to everything under SCAN_ROOTS.  A CGI
# routinely defines its own command names in its own .c or .h (DO_QUERY lives in
# hgIntegrator.c, the arg* names in hg/hubApi/dataApi.h), so the macro table has
# to cover the scanned trees too or half the names come out as {IDENT}.
MACRO_DIRS = ["inc", "hg/inc"]


# ---------------------------------------------------------------------------
# macro table
# ---------------------------------------------------------------------------

def build_macros():
    """Map every #define that resolves to a string literal, chasing aliases.

    Two passes, because the tree defines names in terms of other names:
        #define hgHub          "hgHubConnect."
        #define hgHubDo        hgHub "do_"
        #define hgHubDoClear   hgHubDo "clear"
    The concatenating form is handled by resolving right to left over several
    rounds until nothing new appears.
    """
    macro = {}
    chains = []
    # #define NAME "literal"
    lit_re = re.compile(
        r'^\s*#\s*define\s+([A-Za-z_]\w*)\s+'
        r'("(?:[^"\\]|\\.)*")\s*(?:/[/*].*)?$')
    # #define NAME OTHER, or NAME OTHER "suffix", or NAME "prefix" OTHER
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
                macro.setdefault(m.group(1), m.group(2)[1:-1])
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
                else:
                    out = None
                    break
            if out is not None:
                macro[name] = out
    return macro


def resolve(tok, macro, localconst=None):
    """Turn one C token into the name it stands for, or {ident} if unknown.

    localconst is the file's own char * constants, which take precedence over
    the shared #define table and must never be shared between files: see
    CONST_RE for why.
    """
    tok = tok.strip()
    if not tok:
        return None
    if tok.startswith('"') and tok.endswith('"') and len(tok) >= 2:
        return tok[1:-1]
    if tok == "NULL":
        return None
    if localconst and tok in localconst:
        return localconst[tok]
    if tok in macro:
        return macro[tok]
    if re.match(r'^[A-Za-z_]\w*$', tok):
        return "{%s}" % tok
    return None


# ---------------------------------------------------------------------------
# scanning
# ---------------------------------------------------------------------------

# static char *dbCgiName = "db";  Not every name is a #define: web.c holds db,
# org and clade this way, and they are the most-used URL params there are.
# Resolved per file and never pooled, because the same identifier means
# different things in different programs, and a pooled table let whichever file
# was walked first answer for all of them: five unrelated cartRemove(cart,
# varName) sites were reported as removing "dnaLines", the value an assembly
# tool happens to give its own varName.
#
# The cost is the genuine cross-file case, a const defined in one .c and
# declared extern in a header: snp125ColorSourceOldVar (hg/cgilib/snp125Ui.c)
# now reads as {snp125ColorSourceOldVar} at its hgTrackUi call site.  Pooling
# only extern-declared names would recover it but bring the collisions back,
# since `database` is extern in hgTracks and separately initialized to a
# literal in an unrelated ENCODE tool.  One honest {ident} beats eleven
# confident wrong answers.
CONST_RE = re.compile(
    r'^[ \t]*(?:static[ \t]+)?(?:const[ \t]+)?char[ \t]*\*[ \t]*([A-Za-z_]\w*)'
    r'[ \t]*=[ \t]*("(?:[^"\\]|\\.)*")[ \t]*;', re.M)

EXCLUDE_RE = re.compile(r'\bchar\s*\*\s*excludeVars\s*\[\s*\]\s*=\s*\{')

# cgiOptionalString / cgiUsualString / cgiVarExists / cgiOptionalInt / cgiString
# / cgiBoolean / cgiBooleanDefined / cgiUsualInt ...
CGI_READ_RE = re.compile(
    r'\bcgi(?:Optional|Usual)?'
    r'(?:String|Int|Double|Boolean|BooleanDefined|VarExists)?'
    r'\s*\(\s*("(?:[^"\\]|\\.)*"|[A-Za-z_]\w*)\s*[,)]')

CART_REMOVE_RE = re.compile(
    r'\bcartRemove(?:Prefix|Like)?\s*\(\s*\w+\s*,\s*'
    r'("(?:[^"\\]|\\.)*"|[A-Za-z_]\w*)\s*[,)]')

# Reads that are not URL commands: these fetch the value of a name held in a
# variable, or are the cart's own plumbing.
CGI_READ_SKIP = {"var", "name", "varName", "field", "setting", "track",
                 "cartVar", "booVar", "multVar", "buttonVar", "s", "str"}


def macro_files():
    """Every .h and .c that could hold a #define we need to resolve."""
    for d in MACRO_DIRS:
        p = os.path.join(ROOT, d)
        if not os.path.isdir(p):
            continue
        for fn in sorted(os.listdir(p)):
            if fn.endswith((".h", ".c")):
                yield os.path.join(p, fn)
    for root_rel in SCAN_ROOTS:
        root = os.path.join(ROOT, root_rel)
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
            for fn in sorted(filenames):
                if fn.endswith((".h", ".c")):
                    yield os.path.join(dirpath, fn)


def source_files():
    for root_rel in SCAN_ROOTS:
        root = os.path.join(ROOT, root_rel)
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
            for fn in sorted(filenames):
                if fn.endswith(".c"):
                    yield os.path.join(dirpath, fn)


def rel(path):
    return os.path.relpath(path, ROOT)


def owner(path):
    """Which CGI or library a file belongs to, for grouping."""
    r = rel(path)
    parts = r.split(os.sep)
    if len(parts) >= 2:
        return os.sep.join(parts[:-1])
    return r


def find_exclude_vars(text, path, macro, localconst):
    """Pull the members out of every excludeVars[] declaration in one file."""
    out = []
    for m in EXCLUDE_RE.finditer(text):
        start = m.end()
        depth = 1
        i = start
        instr = False
        while i < len(text) and depth:
            c = text[i]
            if instr:
                if c == "\\":
                    i += 2
                    continue
                if c == '"':
                    instr = False
            elif c == '"':
                instr = True
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
            i += 1
        body = text[start:i-1]
        line = text.count("\n", 0, m.start()) + 1
        # strip comments so a commented-out member is not harvested
        body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
        body = re.sub(r'//[^\n]*', '', body)
        for tok in body.split(","):
            name = resolve(tok, macro, localconst)
            if name:
                out.append((name, "%s:%d" % (rel(path), line)))
    return out


def find_matches(regex, text, path, macro, localconst, skip=()):
    out = []
    for m in regex.finditer(text):
        tok = m.group(1)
        if tok in skip:
            continue
        name = resolve(tok, macro, localconst)
        if not name:
            continue
        line = text.count("\n", 0, m.start()) + 1
        out.append((name, "%s:%d" % (rel(path), line)))
    return out


def harvest():
    macro = build_macros()
    found = {"excludeVars": collections.defaultdict(list),
             "cgiReads": collections.defaultdict(list),
             "cartRemoves": collections.defaultdict(list)}
    for path in source_files():
        try:
            text = open(path, errors="replace").read()
        except OSError:
            continue
        if "excludeVars" not in text and "cgi" not in text \
                and "cartRemove" not in text:
            continue
        who = owner(path)
        localconst = {m.group(1): m.group(2)[1:-1]
                      for m in CONST_RE.finditer(text)}
        for name, src in find_exclude_vars(text, path, macro, localconst):
            found["excludeVars"][who].append((name, src))
        for name, src in find_matches(CGI_READ_RE, text, path, macro,
                                      localconst, CGI_READ_SKIP):
            found["cgiReads"][who].append((name, src))
        for name, src in find_matches(CART_REMOVE_RE, text, path, macro,
                                      localconst):
            found["cartRemoves"][who].append((name, src))
    return found, macro


# ---------------------------------------------------------------------------
# reporting
# ---------------------------------------------------------------------------

def dedupe(pairs):
    """Collapse repeats of the same name, keeping the first site seen."""
    seen = {}
    for name, src in pairs:
        seen.setdefault(name, src)
    return sorted(seen.items())


def report(found, which, out=sys.stdout):
    label = {"excludeVars": "excludeVars[] members",
             "cgiReads": "direct CGI reads",
             "cartRemoves": "cart reads then removed"}[which]
    print("\n=== %s ===" % label, file=out)
    groups = found[which]
    for who in sorted(groups):
        pairs = dedupe(groups[who])
        print("\n%s  (%d)" % (who, len(pairs)), file=out)
        for name, src in pairs:
            print("    %-38s %s" % (name, src), file=out)


def all_names(found):
    names = set()
    for which in found:
        for pairs in found[which].values():
            for name, _ in pairs:
                names.add(name)
    return sorted(names)


def counts(found):
    c = {}
    for which in found:
        names = set()
        for pairs in found[which].values():
            names.update(n for n, _ in pairs)
        c[which] = len(names)
        c[which + "Files"] = len(found[which])
    c["distinctNames"] = len(all_names(found))
    c["unresolved"] = len([n for n in all_names(found) if n.startswith("{")])
    return c


def as_json(found):
    return {which: {who: [{"name": n, "src": s} for n, s in dedupe(pairs)]
                    for who, pairs in groups.items()}
            for which, groups in found.items()}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exclude-vars", action="store_true")
    ap.add_argument("--cgi-reads", action="store_true")
    ap.add_argument("--cart-removes", action="store_true")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--names", action="store_true")
    ap.add_argument("--json")
    args = ap.parse_args()

    found, macro = harvest()

    if args.exclude_vars or args.all:
        report(found, "excludeVars")
    if args.cgi_reads or args.all:
        report(found, "cgiReads")
    if args.cart_removes or args.all:
        report(found, "cartRemoves")
    if args.names:
        for n in all_names(found):
            print(n)
    if args.json:
        with open(args.json, "w") as f:
            json.dump(as_json(found), f, indent=1)
        print("wrote %s" % args.json)

    if not (args.exclude_vars or args.cgi_reads or args.cart_removes
            or args.all or args.names or args.json):
        c = counts(found)
        print("macros resolved   %d" % len(macro))
        for k in sorted(c):
            print("%-18s %s" % (k, c[k]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
