#!/usr/bin/env python3
"""harvestHgConf.py - find hg.conf variables in the kent tree.

The mechanical half of the hg.conf inventory, and a sibling of
harvestUrlCommands.py (#37923) and harvestCartVars.py (#37838).  The curated
catalog next door (hgConfCatalog.py) is reconciled against what this finds.

Everything in hg.conf is read through one of the cfgOption* accessors in
hg/lib/hgConfig.c, so unlike the cart and URL inventories there is a single
choke point and the scan can be close to complete.  What makes it awkward is
that the accessors do not all take the variable name in the same argument:

  cfgOption(name)                      arg 0 is the name
  cfgOptionDefault(name, def)          arg 0 is the name, arg 1 the default
  cfgOptionBooleanDefault(name, def)   arg 0 is the name, arg 1 is TRUE/FALSE
  cfgVal(name)                         arg 0, and errAborts if absent
  cfgOptionEnv(envName, name)          arg 0 is an ENVIRONMENT variable,
                                       arg 1 is the hg.conf name
  cfgOptionEnvDefault(envName, name, def)          likewise
  cfgOption2(prefix, suffix)           the name is "prefix.suffix"
  cfgOptionDefault2(prefix, suffix, def)           likewise

A regex on the first string literal therefore harvests HGDB_USER, an
environment variable that is not an hg.conf setting at all, and misses db.user,
which is.  This script parses the argument list instead.

Profile families.  cfgOption2's prefix is usually a runtime value, not a
literal: jksql.c reads cfgOption2(profileName, "host") where profileName is
whichever database profile the caller asked for.  So db.host, central.host and
archivecentral.host all come from one call site and none of them appears in the
tree as a literal.  These are reported as suffixes under --profiles rather than
as names, because the set of legal prefixes is a property of the mirror's
hg.conf, not of the source.  This is why names like archivecentral.password are
documented in product/ex.hg.conf yet never appear literally in the code.

Release gates.  cfgOptionBooleanDefault is how a feature is shipped dark
(see the "Gating a new feature behind hg.conf" section of the edit-kent-code
notes).  --gates lists every one with its compiled-in default and its call-site
count, which is the work of removing it later.

Ages.  --age dates each variable by walking the history once with
-G'cfgOption' and recording the first commit that added a line reading it, then
mapping that commit's date onto the CGI_VERSION in effect at the time, taken
from the history of hg/inc/versionInfo.h.  Both traversals are slow (~2 minutes
together) so the result is cached; pass --refresh to rebuild it.  This is what
lets hgConfCatalog.py --sunset report a gate's real age instead of a
hand-maintained guess.

Output needs curation.  The scan cannot tell a mirror configuration knob from a
release gate, cannot tell a live variable from one whose feature was deleted
around it, and reports what it cannot resolve as {ident} rather than dropping
it.  Those marks are signal: they are names built at run time.

Usage:
    harvestHgConf.py                    # summary counts
    harvestHgConf.py --names            # flat sorted name list
    harvestHgConf.py --reads            # every read, grouped by owning dir
    harvestHgConf.py --gates            # boolean flags, defaults, call sites
    harvestHgConf.py --profiles         # cfgOption2 suffix families
    harvestHgConf.py --docs             # what product/ex.hg.conf documents
    harvestHgConf.py --age              # first-seen version per variable
    harvestHgConf.py --age --refresh    # rebuild the age cache (slow)
    harvestHgConf.py --json out.json
"""

import argparse
import collections
import json
import os
import re
import subprocess
import sys

# The tree to scan.  KENT_SRC lets a nightly run point at a pristine
# checkout instead of somebody's working tree, where a stray .c file or a
# half-finished edit would show up as a finding.
ROOT = os.environ.get("KENT_SRC") or os.path.expanduser("~/kent/src")

# Walked for call sites.  lib is in here because a few generic settings
# (udc.*, noSqlInj.*) are read from the core library rather than from hg.
SCAN_ROOTS = ["hg", "lib"]

# Not source we care about, and walking them is slow.
SKIP_DIRS = {"htdocs", "js", "tests", "expected", "input", "trackDb",
             "makeDb/doc", "CVS", ".git", "python", "lowelab", "__pycache__"}

# Mined for #define values in addition to everything under SCAN_ROOTS.
MACRO_DIRS = ["inc", "hg/inc"]

# The documented example configs shipped to mirrors.  These are the closest
# thing the tree has to hg.conf documentation today, and the thing the catalog
# is reconciled against.
DOC_FILES = ["product/ex.hg.conf", "product/minimal.hg.conf"]

# Where the release version lives, and the file whose history gives the
# date -> version mapping used by --age.
VERSION_FILE = "hg/inc/versionInfo.h"

CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "hgConfAges.json")

# Bumped when the shape of the cache changes.  A cache written by an older
# version is still usable for dates, so a mismatch is reported rather than
# treated as an error, but the fields added since will be missing.
CACHE_SCHEMA = 2

# "refs #37925", "#37925", "fixes #37925".  Three digits minimum, so a commit
# talking about #10 or a C preprocessor line does not read as a ticket.
TICKET_RE = re.compile(r'#\s*(\d{3,6})')

# How each accessor lays out its arguments.  nameArg is the index of the
# hg.conf name; defArg is the index of the compiled-in default, if any;
# twoPart means the name is arg0 + "." + arg1.
ACCESSORS = {
    "cfgOption":               {"nameArg": 0, "defArg": None},
    "cfgOptionDefault":        {"nameArg": 0, "defArg": 1},
    "cfgOptionBooleanDefault": {"nameArg": 0, "defArg": 1, "boolean": True},
    "cfgVal":                  {"nameArg": 0, "defArg": None, "required": True},
    "cfgOptionEnv":            {"nameArg": 1, "defArg": None, "envArg": 0},
    "cfgOptionEnvDefault":     {"nameArg": 1, "defArg": 2, "envArg": 0},
    "cfgOption2":              {"twoPart": True, "defArg": None},
    "cfgOptionDefault2":       {"twoPart": True, "defArg": 2},
}

# Reads whose name comes from a variable holding some other module's setting
# name.  These are the accessor's own plumbing in hgConfig.c, not settings.
NAME_SKIP = {"name", "varName", "setting", "option", "prefix", "suffix"}

CFG_CALL_RE = re.compile(r'\bcfg(?:Option[A-Za-z0-9]*|Val)\s*\(')
PREFIX_CALL_RE = re.compile(
    r'\bcfg(?:Names|Vals)WithPrefix\s*\(\s*'
    r'("(?:[^"\\]|\\.)*"|[A-Za-z_]\w*)\s*\)')

# Not every settings name is a #define; some files hold one in a file-scope
# char * instead.  These are resolved per file and never pooled, because the
# same identifier means different things in different programs: hgTracks passes
# a runtime `database` to cfgNamesWithPrefix, while docIdView.c has a
# file-scope `char *database = "encpipeline_prod"`.  Pooling them let one
# program's constant answer for every other file, first one walked winning, and
# put a hardcoded MySQL database name in the registry as an hg.conf setting.
CONST_RE = re.compile(
    r'^[ \t]*(?:static[ \t]+)?(?:const[ \t]+)?char[ \t]*\*[ \t]*([A-Za-z_]\w*)'
    r'[ \t]*=[ \t]*("(?:[^"\\]|\\.)*")[ \t]*;', re.M)


# ---------------------------------------------------------------------------
# macro table
# ---------------------------------------------------------------------------

def build_macros():
    """(name -> literal, names defined inconsistently) over the whole tree.

    Same two-pass approach as harvestUrlCommands.py: the tree defines names in
    terms of other names, so concatenating forms are resolved right to left
    over several rounds until nothing new appears.

    And the same reason for the second return value: a pooled table gives a name
    that two files define differently whichever value the filesystem walk
    reached first, so the answer changes between two checkouts of the same
    commit.  Those names resolve to {NAME} unless the file being scanned defines
    them itself.
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
                        conflict.add(name)     # built on an ambiguous piece
                else:
                    out = None
                    break
            if out is not None:
                macro[name] = out
    return macro, conflict


# #define NAME "literal" in the file being scanned.  Its own definition is the
# one that file means, whatever the rest of the tree says.
LOCAL_DEFINE_RE = re.compile(
    r'^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+'
    r'("(?:[^"\\]|\\.)*")[ \t]*(?:/[/*].*)?$', re.M)


def local_defines(text):
    return {m.group(1): m.group(2)[1:-1]
            for m in LOCAL_DEFINE_RE.finditer(text)}


def resolve(tok, macro, localconst=None, conflict=None):
    """Turn one C token into the name it stands for, or {ident} if unknown.

    localconst is the file's own char * constants and #defines, which take
    precedence over the shared #define table and must never be shared between
    files: see CONST_RE for why.  conflict is the set of names the tree defines
    inconsistently; without the file's own definition to go on, those are
    {ident} rather than a guess.
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
    if conflict and tok in conflict:
        return "{%s}" % tok
    if tok in macro:
        return macro[tok]
    if re.match(r'^[A-Za-z_]\w*$', tok):
        return "{%s}" % tok
    return None


# ---------------------------------------------------------------------------
# file walking
# ---------------------------------------------------------------------------

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
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, root_rel)):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
            for fn in sorted(filenames):
                if fn.endswith((".h", ".c")):
                    yield os.path.join(dirpath, fn)


def source_files():
    for root_rel in SCAN_ROOTS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, root_rel)):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
            for fn in sorted(filenames):
                if fn.endswith((".c", ".h")):
                    yield os.path.join(dirpath, fn)


def rel(path):
    return os.path.relpath(path, ROOT)


def owner(path):
    """Which CGI or library a file belongs to, for grouping."""
    parts = rel(path).split(os.sep)
    return os.sep.join(parts[:-1]) if len(parts) >= 2 else rel(path)


# ---------------------------------------------------------------------------
# argument parsing
# ---------------------------------------------------------------------------

def split_args(text, open_paren):
    """Split one C argument list into top-level arguments.

    open_paren indexes the '(' itself.  Returns the argument strings, or None
    if the parens do not close in this file.  Nested calls, strings and escapes
    are tracked so cfgOptionDefault(name, foo(a, b)) yields two arguments.
    """
    args = []
    depth = 0
    cur = []
    i = open_paren
    instr = False
    inchar = False
    while i < len(text):
        c = text[i]
        if instr:
            cur.append(c)
            if c == "\\":
                if i + 1 < len(text):
                    cur.append(text[i + 1])
                i += 2
                continue
            if c == '"':
                instr = False
            i += 1
            continue
        if inchar:
            cur.append(c)
            if c == "\\":
                if i + 1 < len(text):
                    cur.append(text[i + 1])
                i += 2
                continue
            if c == "'":
                inchar = False
            i += 1
            continue
        if c == '"':
            instr = True
            cur.append(c)
        elif c == "'":
            inchar = True
            cur.append(c)
        elif c == "(":
            depth += 1
            if depth > 1:
                cur.append(c)
        elif c == ")":
            depth -= 1
            if depth == 0:
                args.append("".join(cur))
                return [a.strip() for a in args]
            cur.append(c)
        elif c == "," and depth == 1:
            args.append("".join(cur))
            cur = []
        else:
            cur.append(c)
        i += 1
    return None


def func_name(text, open_paren):
    """The identifier immediately before an open paren."""
    j = open_paren - 1
    while j >= 0 and text[j].isspace():
        j -= 1
    end = j + 1
    while j >= 0 and (text[j].isalnum() or text[j] == "_"):
        j -= 1
    return text[j + 1:end]


def strip_comments(text):
    """Blank out comments, preserving offsets so line numbers stay right.

    Newlines are kept so text.count("\\n", 0, pos) is still the line number,
    which matters because a commented-out cfgOption call would otherwise be
    harvested as a live read.
    """
    def blank(m):
        return re.sub(r'[^\n]', ' ', m.group(0))
    text = re.sub(r'/\*.*?\*/', blank, text, flags=re.S)
    text = re.sub(r'//[^\n]*', blank, text)
    return text


# ---------------------------------------------------------------------------
# scanning
# ---------------------------------------------------------------------------

def scan_file(path, macro, found, conflict=None):
    try:
        raw = open(path, errors="replace").read()
    except OSError:
        return
    if "cfgOption" not in raw and "cfgVal" not in raw:
        return
    # hgConfig.c defines the accessors; its own calls are the implementation,
    # not settings reads.  Its cfgVal/cfgOption uses inside other functions are
    # still real, so only the definitions are skipped, by name, below.
    text = strip_comments(raw)
    # The file's own #defines join its char * constants: both are what this
    # file means, whatever the rest of the tree calls the same name.
    localconst = {m.group(1): m.group(2)[1:-1]
                  for m in CONST_RE.finditer(text)}
    localconst.update(local_defines(text))
    who = owner(path)
    for m in CFG_CALL_RE.finditer(text):
        op = m.end() - 1
        fn = func_name(text, op)
        spec = ACCESSORS.get(fn)
        if spec is None:
            continue
        args = split_args(text, op)
        if not args:
            continue
        line = text.count("\n", 0, m.start()) + 1
        src = "%s:%d" % (rel(path), line)
        default = None
        if spec.get("defArg") is not None and len(args) > spec["defArg"]:
            default = args[spec["defArg"]].strip()

        if spec.get("twoPart"):
            if len(args) < 2:
                continue
            # cfgOption2/cfgOptionDefault2 are themselves defined in terms of
            # each other in hgConfig.c, passing their own prefix/suffix
            # parameters through.  Those are the implementation, not a read.
            if args[0] in NAME_SKIP or args[1] in NAME_SKIP:
                continue
            pre = resolve(args[0], macro, localconst, conflict)
            suf = resolve(args[1], macro, localconst, conflict)
            if pre is None or suf is None:
                continue
            if pre.startswith("{"):
                # runtime profile name: record the suffix as a family member
                found["profiles"][suf].append((pre, src, fn))
            else:
                found["reads"][who].append(
                    {"name": "%s.%s" % (pre, suf), "src": src, "func": fn,
                     "default": default})
            continue

        idx = spec["nameArg"]
        if len(args) <= idx:
            continue
        tok = args[idx]
        if tok in NAME_SKIP:
            continue
        name = resolve(tok, macro, localconst, conflict)
        if not name:
            continue
        rec = {"name": name, "src": src, "func": fn, "default": default}
        if spec.get("envArg") is not None and len(args) > spec["envArg"]:
            env = resolve(args[spec["envArg"]], macro, localconst, conflict)
            if env:
                rec["env"] = env
        if spec.get("boolean"):
            rec["boolean"] = True
        if spec.get("required"):
            rec["required"] = True
        found["reads"][who].append(rec)

    for m in PREFIX_CALL_RE.finditer(text):
        if m.group(1) in NAME_SKIP:
            # cfgValsWithPrefix passing its own parameter to cfgNamesWithPrefix
            continue
        name = resolve(m.group(1), macro, localconst, conflict)
        if name:
            line = text.count("\n", 0, m.start()) + 1
            found["prefixScans"][name].append("%s:%d" % (rel(path), line))


def harvest():
    macro, conflict = build_macros()
    found = {"reads": collections.defaultdict(list),
             "profiles": collections.defaultdict(list),
             "prefixScans": collections.defaultdict(list)}
    for path in source_files():
        # The accessors themselves live here; their bodies read the config
        # hash directly and would otherwise show up as reads of {name}.
        if rel(path) in ("hg/lib/hgConfig.c", "hg/inc/hgConfig.h"):
            scan_file(path, macro, found, conflict)
            continue
        scan_file(path, macro, found, conflict)
    return found, macro


def all_reads(found):
    """Every read record, flattened."""
    for recs in found["reads"].values():
        for rec in recs:
            yield rec


def by_name(found):
    """Collapse reads to one record per name, keeping every call site."""
    out = {}
    for rec in all_reads(found):
        d = out.setdefault(rec["name"], {
            "name": rec["name"], "sites": [], "funcs": set(),
            "defaults": set(), "env": None,
            "boolean": False, "required": False})
        d["sites"].append(rec["src"])
        d["funcs"].add(rec["func"])
        if rec.get("default") is not None:
            d["defaults"].add(rec["default"])
        if rec.get("env"):
            d["env"] = rec["env"]
        if rec.get("boolean"):
            d["boolean"] = True
        if rec.get("required"):
            d["required"] = True
    return out


# ---------------------------------------------------------------------------
# the documented example configs
# ---------------------------------------------------------------------------

def parse_doc_files():
    """Names appearing in the shipped example hg.conf files.

    Both live and commented-out assignments count as documented: half of
    ex.hg.conf is deliberately commented out, since it is a menu of what a
    mirror may set rather than a working config.
    """
    docs = {}
    assign_re = re.compile(r'^(#?)\s*([A-Za-z_][A-Za-z0-9_.]*)\s*=')
    for rel_path in DOC_FILES:
        path = os.path.join(ROOT, rel_path)
        if not os.path.exists(path):
            continue
        for num, line in enumerate(open(path, errors="replace"), 1):
            m = assign_re.match(line)
            if not m:
                continue
            name = m.group(2)
            d = docs.setdefault(name, {"name": name, "sites": [],
                                       "commentedOut": True})
            d["sites"].append("%s:%d" % (rel_path, num))
            if not m.group(1):
                d["commentedOut"] = False
    return docs


# ---------------------------------------------------------------------------
# version history, for --age
# ---------------------------------------------------------------------------

def git(*args):
    return subprocess.run(["git", "-C", ROOT] + list(args),
                          capture_output=True, text=True).stdout


def version_timeline():
    """[(timestamp, version)] for every CGI_VERSION bump, oldest first.

    Read out of the history of hg/inc/versionInfo.h rather than hardcoded, so
    it stays right as releases happen.
    """
    out = git("log", "--format=%at", "-p", "--follow", VERSION_FILE)
    stamps = []
    ts = None
    for line in out.splitlines():
        if re.match(r'^\d{9,}$', line):
            ts = int(line)
        elif line.startswith("+#define CGI_VERSION"):
            m = re.search(r'"(\d+)"', line)
            if m and ts is not None:
                stamps.append((ts, int(m.group(1))))
    stamps.sort()
    return stamps


def version_at(ts, timeline):
    """The CGI_VERSION in development when ts happened.

    A commit lands after version N is stamped and ships in N+1, so the version
    a variable was introduced in is the one stamped at or before its commit.
    """
    ver = None
    for stamp, v in timeline:
        if stamp <= ts:
            ver = v
        else:
            break
    return ver


def current_version():
    path = os.path.join(ROOT, VERSION_FILE)
    m = re.search(r'CGI_VERSION\s+"(\d+)"', open(path).read())
    return int(m.group(1)) if m else None


def commit_messages(shas):
    """sha -> full commit message, in batches so this is a few git calls."""
    msgs = {}
    shas = sorted(set(shas))
    for i in range(0, len(shas), 200):
        # \x01 between records and \x02 between the hash and the body, so a
        # multi-line commit message can be split back apart safely.
        raw = git("show", "-s", "--format=%x01%H%x02%B", *shas[i:i + 200])
        for rec in raw.split("\x01"):
            if not rec.strip():
                continue
            sha, _, body = rec.partition("\x02")
            msgs[sha.strip()] = body
    return msgs


def tickets_in(msg):
    return sorted({int(m) for m in TICKET_RE.findall(msg or "")})


def walk_adds(pattern, matcher, paths=("hg", "lib")):
    """Every commit that added a line matching, oldest first, per name.

    One -G traversal.  matcher(line) returns the names that line introduces.
    Returns {name: [(timestamp, sha)]} with consecutive duplicates collapsed,
    so the first entry is the introducing commit and the rest are later
    commits that touched a read of the same name.
    """
    out = git("log", "--reverse", "-G", pattern, "--format=COMMIT %at %H",
              "-p", "--unified=0", "--", *paths)
    hits = {}
    ts = sha = None
    for line in out.splitlines():
        if line.startswith("COMMIT "):
            f = line.split()
            if len(f) >= 3:
                ts, sha = int(f[1]), f[2]
            continue
        if not line.startswith("+") or ts is None:
            continue
        for name in matcher(line):
            lst = hits.setdefault(name, [])
            if not lst or lst[-1][1] != sha:
                lst.append((ts, sha))
    return hits


def harvest_ages(names=None, refresh=False):
    """When each hg.conf variable arrived, when a flag flipped, and under
    which ticket.

    Two history traversals.  The first is filtered on -G'cfgOption' and
    records, for each name, the earliest commit that added a line reading it,
    plus for boolean flags the earliest commit that added a read with a TRUE
    compiled-in default.  That second date is what turns a release gate's
    lifecycle into something the tree knows rather than something a person
    maintains by hand: a gate is introduced defaulting FALSE, flips to TRUE
    when the feature ships, and only the removal deadline is left as a
    judgement call.

    The second traversal exists because a read whose name comes from a macro
    is invisible to the first.  cfgOptionBooleanDefault(CFG_LOGIN_HTTPS, ...)
    carries no literal, so login.https is dated from the #define instead, by
    walking the names the first pass missed.  That covers 274 of the 281
    catalogued settings; the remainder are the names built at run time, which
    have no single birthday to find.

    Attribution.  Each commit message is searched for a Redmine ticket, and
    only two commits are allowed to speak for a setting: the one that added
    the read, and for a flag the one that turned its default on.  Nothing
    else is used, and the reason is worth writing down.  An earlier draft
    fell back to the next later commit that touched the same read, which
    tripled coverage and was wrong: it credited wiki.host and textSize to
    #37838, a 2026 cart refactor that happened to touch those lines.  A
    commit that edits a line is not a commit about the setting on it.

    So roughly 40% of settings get a ticket and the rest honestly have none.
    Of those, about half were added before the tree used Redmine at all and
    can never have one.  The rest are recent enough that somebody chose not
    to cite a ticket, and --tickets lists them separately as the ones a human
    could still fill in.

    About four minutes for both walks, so the result is cached next to this
    script.

    Caveats.  This dates the earliest surviving read of a name, which is the
    right question for "how long has this been in the tree", but a name
    removed and later reintroduced dates from the reintroduction.  A flag
    whose default flipped TRUE and then back to FALSE still reports the first
    flip.  A ticket number in a commit message is whatever the committer
    typed, so a typo becomes a wrong ticket here.
    """
    if not refresh and os.path.exists(CACHE):
        with open(CACHE) as f:
            ages = json.load(f)
        # The release version must come from the tree, never from the cache.
        # Every deadline in the sunset report is arithmetic against it, so a
        # cache built two releases ago would quietly move every deadline two
        # releases into the future.  Keep the build-time value under cachedAt
        # so staleness can be reported rather than guessed at.
        ages["cachedAt"] = ages.get("current")
        ages["current"] = current_version()
        ages["stale"] = (ages["cachedAt"] is not None
                         and ages["current"] is not None
                         and ages["cachedAt"] < ages["current"])
        ages["oldSchema"] = ages.get("schema", 1) < CACHE_SCHEMA
        return ages

    timeline = version_timeline()

    # Pass one: any literal in a cfgOption* call on an added line.
    # Deliberately looser than the real scan, since here a false positive only
    # mis-dates a name while a miss loses it entirely.
    call_re = re.compile(r'cfg(?:Option[A-Za-z0-9]*|Val)\s*\(\s*'
                         r'(?:"[^"]*"\s*,\s*)?"([^"]+)"')
    # cfgOptionBooleanDefault("name", TRUE) specifically, for the flip date.
    true_re = re.compile(r'cfgOptionBooleanDefault\s*\(\s*"([^"]+)"\s*,\s*'
                         r'(TRUE|1)\s*\)')
    # Both come out of the same traversal.  A flip is tagged with a prefix a
    # setting name cannot contain, then split back out below, so that turning
    # this into two walks costs nothing.
    def reads_and_flips(line):
        return (call_re.findall(line)
                + ["\x00" + n for n, _ in true_re.findall(line)])

    walked = walk_adds("cfgOption", reads_and_flips)
    hits = {n: v for n, v in walked.items() if not n.startswith("\x00")}
    flips = {n[1:]: v for n, v in walked.items() if n.startswith("\x00")}

    # Pass two: the catalogued names pass one never saw, matched as a quoted
    # literal anywhere, which finds the macro definition that names them.
    chased = {}
    todo = sorted(n for n in (names or [])
                  if n not in hits and not n.startswith("{"))
    if todo:
        alt = "|".join(re.escape(n) for n in
                       sorted(todo, key=len, reverse=True))
        lit = {n: re.compile('"' + re.escape(n) + '"') for n in todo}
        chased = walk_adds(alt, lambda line: [n for n, p in lit.items()
                                              if p.search(line)])

    msgs = commit_messages([s for lst in hits.values() for _, s in lst]
                           + [s for lst in flips.values() for _, s in lst]
                           + [s for lst in chased.values() for _, s in lst])

    def record(lst, via):
        ts, sha = lst[0]
        return {"ts": ts, "version": version_at(ts, timeline),
                "commit": sha[:11], "tickets": tickets_in(msgs.get(sha)),
                "subject": (msgs.get(sha, "").splitlines() or [""])[0][:120],
                "via": via}

    first = {n: record(lst, "call") for n, lst in hits.items()}
    for n, lst in chased.items():
        first[n] = record(lst, "literal")

    ages = {"schema": CACHE_SCHEMA,
            "current": current_version(),
            "timeline": timeline,
            "first": first,
            "firstTrue": {n: record(lst, "call") for n, lst in flips.items()}}
    with open(CACHE, "w") as f:
        json.dump(ages, f, indent=1, sort_keys=True)
    return ages


def ticket_for(name, ages):
    """(tickets, kind) for a setting, where kind says what the tickets are.

    kind is "introduced" when the commit that added the read cites tickets,
    "flip" when only the commit that turned the flag's default on does, and
    None when neither names one.  The list is every ticket that commit cited,
    kept whole rather than reduced to the first, because a commit citing three
    tickets has not told us which one asked for the setting.
    """
    rec = (ages.get("first") or {}).get(name) or {}
    if rec.get("tickets"):
        return rec["tickets"], "introduced"
    flip = (ages.get("firstTrue") or {}).get(name) or {}
    if flip.get("tickets"):
        return flip["tickets"], "flip"
    return [], None


def cite(tickets, kind):
    """Render a ticket list for a plain-text report."""
    if not tickets:
        return ""
    s = ", ".join("#%d" % t for t in tickets)
    return s if kind == "introduced" else s + " (flip)"


# ---------------------------------------------------------------------------
# reporting
# ---------------------------------------------------------------------------

def report_reads(found, out=sys.stdout):
    print("\n=== hg.conf reads, by owning directory ===", file=out)
    for who in sorted(found["reads"]):
        recs = {}
        for rec in found["reads"][who]:
            recs.setdefault(rec["name"], rec)
        print("\n%s  (%d)" % (who, len(recs)), file=out)
        for name in sorted(recs):
            rec = recs[name]
            tail = rec["func"]
            if rec.get("default") is not None:
                tail += " default=%s" % rec["default"]
            print("    %-40s %-28s %s" % (name, rec["src"], tail), file=out)


def report_gates(found, ages, out=sys.stdout):
    """Boolean flags, their compiled-in default, age and call-site count."""
    names = by_name(found)
    gates = {n: d for n, d in names.items() if d["boolean"]}
    print("\n=== cfgOptionBooleanDefault flags (%d) ===" % len(gates), file=out)
    cur = ages.get("current") if ages else None
    print("%-32s %-7s %5s %8s %8s  %s" %
          ("name", "default", "sites", "added", "flipped", "first site"),
          file=out)
    for name in sorted(gates):
        d = gates[name]
        defs = ",".join(sorted(d["defaults"])) or "?"
        info = (ages or {}).get("first", {}).get(name)
        added = ""
        if info and info.get("version"):
            added = "v%s" % info["version"]
            if cur:
                added += "(%d)" % (cur - info["version"])
        flip = (ages or {}).get("firstTrue", {}).get(name)
        flipped = ""
        if flip and flip.get("version"):
            flipped = "v%s" % flip["version"]
        print("%-32s %-7s %5d %8s %8s  %s" %
              (name, defs, len(d["sites"]), added, flipped,
               sorted(d["sites"])[0]), file=out)
    if cur:
        print("\nadded: version first seen, and how many releases ago.",
              file=out)
        print("flipped: version its compiled-in default first became TRUE.",
              file=out)


def report_profiles(found, out=sys.stdout):
    print("\n=== cfgOption2 suffix families ===", file=out)
    print("These are read as <profile>.<suffix> where the profile name is a "
          "runtime\nvalue, so the legal prefixes come from the mirror's "
          "hg.conf, not the source.\n", file=out)
    for suf in sorted(found["profiles"]):
        entries = found["profiles"][suf]
        prefixes = sorted({p for p, _, _ in entries})
        print("    %-22s %d sites  prefix from %s" %
              (suf, len(entries), ", ".join(prefixes)), file=out)
    if found["prefixScans"]:
        print("\n=== cfgNamesWithPrefix / cfgValsWithPrefix ===", file=out)
        for name in sorted(found["prefixScans"]):
            print("    %-22s %s" % (name,
                  ", ".join(sorted(set(found["prefixScans"][name])))), file=out)


def report_docs(found, out=sys.stdout):
    """Compare the code against the shipped example configs."""
    docs = parse_doc_files()
    names = by_name(found)
    code = {n for n in names if not n.startswith("{")}
    documented = set(docs)
    print("\n=== product/ex.hg.conf vs the code ===", file=out)
    print("\nread by code, not in the example configs (%d):"
          % len(code - documented), file=out)
    for n in sorted(code - documented):
        print("    %-40s %s" % (n, sorted(names[n]["sites"])[0]), file=out)
    print("\nin the example configs, no literal read found (%d):"
          % len(documented - code), file=out)
    print("    (expect profile-family members here: archivecentral.password "
          "and\n     friends are read through cfgOption2 and never appear "
          "literally)", file=out)
    for n in sorted(documented - code):
        print("    %-40s %s" % (n, docs[n]["sites"][0]), file=out)


def report_ages(found, ages, out=sys.stdout):
    names = by_name(found)
    cur = ages.get("current")
    first = ages.get("first", {})
    print("\n=== first version seen (current tree: v%s) ===" % cur, file=out)
    known = [(first[n]["version"], n) for n in sorted(names)
             if n in first and first[n].get("version")]
    known.sort()
    for ver, name in known:
        tickets, kind = ticket_for(name, ages)
        print("    v%-5s %-42s %-22s %d sites"
              % (ver, name, cite(tickets, kind),
                 len(names[name]["sites"])), file=out)
    missing = [n for n in sorted(names)
               if not n.startswith("{") and n not in first]
    if missing:
        print("\nnot datable from history (%d): read through a macro or "
              "renamed" % len(missing), file=out)
        for n in missing:
            print("    %s" % n, file=out)
    dated = [n for _, n in known]
    intro = [n for n in dated if ticket_for(n, ages)[1] == "introduced"]
    print("\n%d of %d dated names cite a ticket in the commit that added them.  "
          "See --tickets\nfor the rest, which mostly predate the tree's use of "
          "Redmine." % (len(intro), len(dated)), file=out)


# The tree started citing Redmine tickets in commit messages around here.  A
# setting older than this cannot have one, so it is reported as out of scope
# rather than as a gap somebody should go and fill.
REDMINE_ERA = 270


def report_tickets(found, ages, out=sys.stdout):
    """Which ticket introduced each setting, and where that is not knowable.

    Only the commit that added the read, and for a flag the commit that turned
    its default on, are allowed to answer.  See harvest_ages for why nothing
    looser is used.
    """
    names = by_name(found)
    first = ages.get("first", {})
    rows = []
    for name in sorted(names):
        if name.startswith("{"):
            continue
        tickets, kind = ticket_for(name, ages)
        rows.append({"name": name, "tickets": tickets, "kind": kind,
                     "version": first.get(name, {}).get("version"),
                     "commit": first.get(name, {}).get("commit")})

    def show(sel):
        for r in sorted(sel, key=lambda r: (r["version"] or 0, r["name"])):
            print("    %-42s %-7s %-22s %s"
                  % (r["name"], "v%s" % r["version"] if r["version"] else "?",
                     cite(r["tickets"], r["kind"]), r["commit"] or ""),
                  file=out)

    print("\n=== the ticket that introduced each setting ===", file=out)
    print("\nOnly two commits get to answer for a setting: the one that added "
          "the read,\nand for a flag the one that turned its default on.",
          file=out)

    have = [r for r in rows if r["kind"] == "introduced"]
    print("\nATTRIBUTED, from the commit that added the read: %d" % len(have),
          file=out)
    show(have)

    flip = [r for r in rows if r["kind"] == "flip"]
    print("\nATTRIBUTED, only from the commit that turned the default on: %d"
          % len(flip), file=out)
    print("  The ticket that shipped the feature, which is not necessarily "
          "the one that\n  asked for the flag.", file=out)
    show(flip)

    # A dated name with no version is older than the first CGI_VERSION stamp
    # the timeline reaches, so it belongs here rather than nowhere.
    old = [r for r in rows if not r["kind"]
           and (r["version"] or 0) < REDMINE_ERA]
    print("\nBEFORE REDMINE (added before ~v%d, so there is no ticket to "
          "find): %d" % (REDMINE_ERA, len(old)), file=out)
    show(old)

    gap = [r for r in rows if not r["kind"]
           and (r["version"] or 0) >= REDMINE_ERA]
    print("\nNO TICKET CITED (added late enough that there probably was one): "
          "%d" % len(gap), file=out)
    print("  This is the actionable list.  Each of these was added by a commit "
          "that names\n  no ticket, so the only way to fill it in is somebody "
          "who remembers, or a\n  search of Redmine for the setting name.",
          file=out)
    show(gap)

    undated = [n for n in sorted(names)
               if not n.startswith("{") and n not in first]
    if undated:
        print("\nNO BIRTHDAY TO FIND (the name is built at run time): %d\n    %s"
              % (len(undated), ", ".join(undated)), file=out)
    runtime = [n for n in sorted(names) if n.startswith("{")]
    if runtime:
        print("\nNot settings, so not attributed: %s" % ", ".join(runtime),
              file=out)


def counts(found):
    names = by_name(found)
    resolved = [n for n in names if not n.startswith("{")]
    return {
        "distinctNames": len(resolved),
        "unresolved": len(names) - len(resolved),
        "booleanFlags": len([n for n, d in names.items() if d["boolean"]]),
        "required": len([n for n, d in names.items() if d["required"]]),
        "envOverridable": len([n for n, d in names.items() if d["env"]]),
        "profileSuffixes": len(found["profiles"]),
        "prefixScans": len(found["prefixScans"]),
        "owningDirs": len(found["reads"]),
        "documented": len(parse_doc_files()),
    }


def as_json(found, ages):
    names = by_name(found)
    docs = parse_doc_files()
    first = (ages or {}).get("first", {})
    out = {}
    for name in sorted(names):
        d = names[name]
        rec = {"name": name,
               "sites": sorted(d["sites"]),
               "funcs": sorted(d["funcs"]),
               "defaults": sorted(d["defaults"]),
               "boolean": d["boolean"],
               "required": d["required"],
               "documented": name in docs}
        if d["env"]:
            rec["env"] = d["env"]
        if name in first:
            rec["firstVersion"] = first[name].get("version")
            rec["firstCommit"] = first[name].get("commit")
        flip = (ages or {}).get("firstTrue", {}).get(name)
        if flip:
            rec["flippedVersion"] = flip.get("version")
            rec["flippedCommit"] = flip.get("commit")
        if ages:
            tickets, kind = ticket_for(name, ages)
            if tickets:
                rec["tickets"] = tickets
                rec["ticketFrom"] = kind
        out[name] = rec
    return {"names": out,
            "profiles": {s: sorted({p for p, _, _ in v})
                         for s, v in found["profiles"].items()},
            "prefixScans": {k: sorted(set(v))
                            for k, v in found["prefixScans"].items()},
            "documentedOnly": sorted(set(docs) - set(names)),
            "currentVersion": (ages or {}).get("current")}


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--names", action="store_true")
    ap.add_argument("--reads", action="store_true")
    ap.add_argument("--gates", action="store_true")
    ap.add_argument("--profiles", action="store_true")
    ap.add_argument("--docs", action="store_true")
    ap.add_argument("--age", action="store_true")
    ap.add_argument("--tickets", action="store_true",
                    help="the ticket that introduced each setting")
    ap.add_argument("--refresh", action="store_true",
                    help="rebuild the age cache (walks history, ~4 minutes)")
    ap.add_argument("--json")
    args = ap.parse_args()

    found, macro = harvest()

    ages = None
    if args.age or args.gates or args.tickets or args.json or args.refresh:
        # The scanned names let the refresh chase the ones whose reads name
        # them through a macro, which the cfgOption filter cannot see.
        ages = harvest_ages(names=sorted(by_name(found)),
                            refresh=args.refresh)

    if args.reads:
        report_reads(found)
    if args.gates:
        report_gates(found, ages)
    if args.profiles:
        report_profiles(found)
    if args.docs:
        report_docs(found)
    if args.age:
        report_ages(found, ages)
    if args.tickets:
        report_tickets(found, ages)
    if args.names:
        for n in sorted(by_name(found)):
            print(n)
    if args.json:
        with open(args.json, "w") as f:
            json.dump(as_json(found, ages), f, indent=1, sort_keys=True)
        print("wrote %s" % args.json)

    if not any([args.reads, args.gates, args.profiles, args.docs, args.age,
                args.tickets, args.names, args.json]):
        c = counts(found)
        print("macros resolved   %d" % len(macro))
        for k in sorted(c):
            print("%-18s %s" % (k, c[k]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
