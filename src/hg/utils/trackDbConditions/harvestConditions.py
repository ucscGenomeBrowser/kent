#!/usr/bin/env python3
"""harvestConditions.py - find where the browser reads a trackDb setting, and what has to be true first.

Refs #37908.  A setting's row in trackDbLibrary.shtml says which track types it
applies to.  It is a flat list, so it cannot say "only in coverage mode", "only
when the track is in pack", "only when another setting is on".  Those conditions
are real and there are a lot of them.  This finds them by reading the C.

Two questions, kept apart, because they are different questions:

  render   what has to be true for the setting to change the picture.  Scanned
           over hg/hgTracks.  This is the one the documentation is about.
  config   what has to be true for the control to appear on the track's
           configuration page.  Scanned over hui.c and the *Ui.c files.

How a condition is found
------------------------
Two passes.  The first records, for every read of a setting, the if/else/while
/for/switch conditions whose block encloses it.  Ranges are used rather than
brace depth, so `if (x) return;` is handled as exactly as `if (x) { ... }`, and
an early return contributes its negation to the rest of the function, which is
where a lot of kent's real guards live.

That alone finds little, because the read itself is usually plain and the
condition sits at the caller.  hicUiGetArcLimit() just reads the setting; the
test for arc mode is up in hicTrack.c.  So the second pass builds a call graph
and, for each function, works out the conditions that hold on EVERY path into
it.

Only conditions true on every path are reported.  That keeps the claim sound: a
reported condition is necessary, never sufficient.  Two things make a function
give up and report nothing rather than something false:

  - its address is taken somewhere, so it can be a track method pointer and be
    reached from outside anything this scanner can see.
  - it is recursive, or nothing in scope calls it.

What it cannot see
------------------
A name built with safef rather than written at the call site.  A condition
carried in a variable rather than tested in place: bamColorTag is read plainly
and the test for bamColorMode==tag is at the point the value is used, several
lines later, which is a def-use question this does not ask.  Conditions in
JavaScript.  So the output is a floor, not a census.
"""

import collections
import glob
import json
import os
import re
import sys

# The argument that holds the setting name, per reader.  A reader not listed
# here is not a setting read.
#
# TDB_READERS take a trackDb, so what they read is a track setting and can carry
# a trackDb default.  The rest read a plain cart variable belonging to the page:
# chromInfoPage, the track search box, which configuration tab is open.  Those
# are real cart variables and #37923 catalogs them, but they are not trackDb
# settings and mixing them in buries the signal.
TDB_READERS = {
    "cartOrTdbBoolean", "cartOrTdbString", "cartOrTdbInt", "cartOrTdbDouble",
    "trackDbSetting", "trackDbSettingOrDefault", "trackDbSettingClosestToHome",
    "trackDbSettingClosestToHomeOrDefault", "trackDbFloatSettingOrDefault",
    "trackDbSettingOn", "trackDbSettingOnOrDefault",
    "cartUsualStringClosestToHome", "cartUsualIntClosestToHome",
    "cartUsualBooleanClosestToHome", "cartUsualDoubleClosestToHome",
    "cartStringClosestToHome", "cartBooleanClosestToHome",
}

READERS = {
    "cartOrTdbBoolean": 2, "cartOrTdbString": 2, "cartOrTdbInt": 2, "cartOrTdbDouble": 2,
    "trackDbSetting": 1, "trackDbSettingOrDefault": 1, "trackDbSettingClosestToHome": 1,
    "trackDbSettingClosestToHomeOrDefault": 1, "trackDbFloatSettingOrDefault": 1,
    "trackDbSettingOn": 1, "trackDbSettingOnOrDefault": 1,
    "cartUsualStringClosestToHome": 3, "cartUsualIntClosestToHome": 3,
    "cartUsualBooleanClosestToHome": 3, "cartUsualDoubleClosestToHome": 3,
    "cartStringClosestToHome": 2, "cartBooleanClosestToHome": 2,
    "cartUsualString": 1, "cartUsualInt": 1, "cartUsualBoolean": 1, "cartUsualDouble": 1,
    "cartOptionalString": 1, "cartString": 1, "cartInt": 1, "cartBoolean": 1,
    "cartVarExists": 1,
}

KEYWORDS = ("if", "while", "for", "switch")
NOT_A_CALL = set(KEYWORDS) | {"else", "return", "sizeof", "case", "defined", "do"}

# The two scopes.  A file can be in both: wiggleCart.c is read from the drawing
# code and from the config page, and the answer differs.
SCOPES = {
    "render": ["hg/hgTracks/*.c"],
    "config": ["hg/hgTrackUi/*.c", "hg/hgTracks/config.c", "hg/hgTracks/searchTracks.c"],
}

# In hgTracks but not drawing: the configuration page and the track search page
# both read track settings, and neither is the picture.
NOT_RENDER = ("hg/hgTracks/config.c", "hg/hgTracks/searchTracks.c")

# Scanned for both scopes.  hui.c and the *Ui.c files are not only the config
# page: they also hold the accessors the drawing code calls, so hicUiGetArcLimit
# has to be visible to the render graph as well as the config one.  Which scope
# a read belongs to is then decided by who calls it, not by which file it is in.
SHARED = ["hg/lib/hui.c", "hg/lib/*Ui.c", "hg/lib/wiggleCart.c",
          "hg/lib/trackDbCustom.c", "hg/lib/hdb.c"]

# A read reached with none of these is unguarded.  Anything matching is noise
# rather than a condition on the setting: it says the code got far enough to
# run, not that the setting only applies sometimes.
NOISE = re.compile(
    r"^NOT \((?:[\w>.\-]+(?:\s*->\s*\w+)* == NULL)"
    r"(?:\s*\|\|\s*[\w>.\-]+(?:\s*->\s*\w+)* == NULL)*\)$"
    r"|^errCatchStart\b"
    r"|^\w+ != NULL$"
    r"|^NOT \(isEmpty\("
    r"|^NOT \(!\w+\)$"
    r"|^\w+ = \w+; \w+ != NULL"          # for (x = y; x != NULL; ...)
)


def blankOut(src):
    """Comments and string bodies become spaces, so offsets and lines still line up."""
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i+1] == "/":
            while i < n and src[i] != "\n":
                out[i] = " "
                i += 1
        elif c == "/" and i + 1 < n and src[i+1] == "*":
            out[i] = out[i+1] = " "
            i += 2
            while i + 1 < n and not (src[i] == "*" and src[i+1] == "/"):
                if src[i] != "\n":
                    out[i] = " "
                i += 1
            if i + 1 < n:
                out[i] = out[i+1] = " "
                i += 2
        elif c in "\"'":
            quote = c
            i += 1
            while i < n and src[i] != quote:
                if src[i] == "\\":
                    out[i] = " "
                    i += 1
                    if i < n and src[i] != "\n":
                        out[i] = " "
                    i += 1
                    continue
                if src[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                i += 1
        else:
            i += 1
    return "".join(out)


def matchParen(s, i):
    """i is at '('.  Index just past the matching ')'."""
    depth = 0
    while i < len(s):
        if s[i] == "(":
            depth += 1
        elif s[i] == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return len(s)


def skipSpace(s, i):
    while i < len(s) and s[i] in " \t\n\r":
        i += 1
    return i


def stmtEnd(s, i):
    """End of the one statement, or block, that starts at i."""
    i = skipSpace(s, i)
    depth = 0
    while i < len(s):
        c = s[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth <= 0:
                return i + 1
        elif c == ";" and depth == 0:
            return i + 1
        i += 1
    return len(s)


def lineIndex(src):
    idx, line = [0] * (len(src) + 1), 1
    for i, c in enumerate(src):
        idx[i] = line
        if c == "\n":
            line += 1
    idx[len(src)] = line
    return idx


LEAVES = re.compile(r"^\s*\{?\s*(return\b|errAbort\s*\(|continue\b|break\b)")


def loadDefines(paths):
    """#define NAME "literal", so a macro-spelled setting is not missed.

    Also follows a macro defined as another macro.  GRAY_LEVEL_SCORE_MIN is
    SCORE_MIN is "scoreMin", and stopping at the first hop loses the read.
    """
    out, alias = {}, {}
    for path in paths:
        try:
            with open(path, errors="replace") as f:
                for line in f:
                    m = re.match(r'\s*#define\s+([A-Z][A-Z0-9_]*)\s+"([^"]*)"', line)
                    if m:
                        out.setdefault(m.group(1), m.group(2))
                        continue
                    m = re.match(r"\s*#define\s+([A-Z][A-Z0-9_]*)\s+([A-Z][A-Z0-9_]*)\s*$", line)
                    if m:
                        alias.setdefault(m.group(1), m.group(2))
        except OSError:
            pass
    for _ in range(8):                       # a chain longer than this is a loop
        grew = False
        for name, target in alias.items():
            if name not in out and target in out:
                out[name] = out[target]
                grew = True
        if not grew:
            break
    return out


def settingNameOf(arg, defines):
    """The setting an argument names: a literal, or a macro that expands to one."""
    arg = arg.strip()
    lit = re.match(r'^"((?:[^"\\]|\\.)*)"$', arg)
    if lit:
        return lit.group(1)
    if arg in defines:
        return defines[arg]
    if re.fullmatch(r"[A-Z][A-Z0-9_]*", arg):
        return "{%s}" % arg
    return None


def splitArgs(text):
    out, depth, cur = [], 0, ""
    for ch in text:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return out


def scanFile(path, kentSrc, defines):
    """Every function, guard, setting read and call in one file."""
    with open(path, errors="replace") as f:
        src = f.read()
    s = blankOut(src)
    line = lineIndex(src)
    rel = os.path.relpath(path, kentSrc)
    n = len(s)

    # functions: name ( ... ) { with the paren at brace depth 0
    funcs = []
    depth, i = 0, 0
    while i < n:
        c = s[i]
        if c == "{":
            depth += 1
            i += 1
            continue
        if c == "}":
            depth -= 1
            i += 1
            continue
        m = re.match(r"[A-Za-z_][A-Za-z0-9_]*", s[i:])
        if not m:
            i += 1
            continue
        word = m.group(0)
        j = skipSpace(s, i + len(word))
        if depth == 0 and j < n and s[j] == "(" and word not in NOT_A_CALL:
            end = matchParen(s, j)
            p = skipSpace(s, end)
            if p < n and s[p] == "{":
                funcs.append((p, stmtEnd(s, p), word))
        i += len(word)

    def enclosing(off):
        return next((f[2] for f in funcs if f[0] <= off < f[1]), None)

    # char *scoreMinStr = trackDbSettingClosestToHome(tdb, GRAY_LEVEL_SCORE_MIN);
    # A later test of scoreMinStr is a test of the setting, not housekeeping, so
    # remember which local holds which setting.
    fromSetting = {}
    for m in re.finditer(r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(", s):
        reader = m.group(2)
        if reader not in READERS:
            continue
        open_ = s.index("(", m.end(2))
        close = matchParen(s, open_)
        args = splitArgs(src[open_ + 1:close - 1])
        idx = READERS[reader]
        if idx >= len(args):
            continue
        name = settingNameOf(args[idx], defines)
        if name and not name.startswith("{"):
            fromSetting[(enclosing(m.start(1)), m.group(1))] = name

    # guards
    guards = []
    i = 0
    while i < n:
        m = re.match(r"\b(if|while|for|switch)\b", s[i:])
        if m and (i == 0 or not (s[i-1].isalnum() or s[i-1] == "_")):
            word = m.group(1)
            j = skipSpace(s, i + len(word))
            if j < n and s[j] == "(":
                close = matchParen(s, j)
                cond = re.sub(r"\s+", " ", src[j+1:close-1]).strip()
                end = stmtEnd(s, close)
                guards.append((close, end, word, cond, line[i]))
                p = skipSpace(s, end)
                if s[p:p+4] == "else" and not (p+4 < n and (s[p+4].isalnum() or s[p+4] == "_")):
                    q = skipSpace(s, p + 4)
                    guards.append((q, stmtEnd(s, q), "else", "NOT (%s)" % cond, line[p]))
                elif word == "if" and LEAVES.match(src[close:end]):
                    # the guarded statement leaves, so the rest of the function
                    # is only reached when the condition is false
                    for fs, fe, _ in funcs:
                        if fs <= i < fe:
                            guards.append((end, fe, "guard", "NOT (%s)" % cond, line[i]))
                            break
                i = close
                continue
        i += 1

    def condsAt(off):
        out = []
        for g in guards:
            if g[0] <= off < g[1]:
                func = enclosing(g[0])
                for part in splitConjuncts(g[3]):
                    derived = sorted({fromSetting[(func, ident)]
                                      for ident in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", part)
                                      if (func, ident) in fromSetting})
                    out.append({"kind": g[2], "text": part, "line": g[4], "file": rel,
                                "derivedFrom": derived})
        return out

    # reads and calls, both only inside a function body
    reads, calls, addrTaken = [], [], set()
    depth, i = 0, 0
    while i < n:
        c = s[i]
        if c == "{":
            depth += 1
            i += 1
            continue
        if c == "}":
            depth -= 1
            i += 1
            continue
        m = re.match(r"[A-Za-z_][A-Za-z0-9_]*", s[i:])
        if not m:
            i += 1
            continue
        word = m.group(0)
        j = skipSpace(s, i + len(word))
        isCall = j < n and s[j] == "("
        if isCall and word in READERS and depth > 0:
            close = matchParen(s, j)
            args = splitArgs(src[j+1:close-1])
            idx = READERS[word]
            name = settingNameOf(args[idx], defines) if idx < len(args) else None
            if name:
                reads.append({"file": rel, "line": line[i], "reader": word, "name": name,
                              "tdb": word in TDB_READERS,
                              "func": enclosing(i), "conds": condsAt(i)})
            i = close
            continue
        if isCall and word not in NOT_A_CALL and depth > 0:
            # depth > 0 keeps a definition or a prototype from counting as a call
            calls.append({"callee": word, "caller": enclosing(i), "file": rel,
                          "line": line[i], "conds": condsAt(i)})
            i += len(word)
            continue
        if not isCall and depth > 0 and re.fullmatch(r"[a-z][A-Za-z0-9_]*", word):
            # only a real use as a value, which is how a track method is
            # installed: tg->drawItems = bedDrawItems;  or  &someFunc
            before = src[max(0, i-2):i].strip()
            after = s[skipSpace(s, i + len(word)):][:1]
            if (before.endswith("=") or before.endswith("&") or before.endswith(",")) \
                    and after in (";", ",", ")"):
                addrTaken.add(word)
        i += len(word)

    return {"file": rel, "funcs": [f[2] for f in funcs], "reads": reads,
            "calls": calls, "addrTaken": sorted(addrTaken)}


def splitConjuncts(text):
    """A && B is two conditions.  Splitting it is sound; splitting || is not."""
    parts, depth, cur = [], 0, ""
    i = 0
    while i < len(text):
        c = text[i]
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        if depth == 0 and text[i:i+2] == "&&":
            parts.append(cur)
            cur = ""
            i += 2
            continue
        cur += c
        i += 1
    parts.append(cur)
    parts = [p.strip() for p in parts if p.strip()]
    return parts if len(parts) > 1 else [text]


def condKey(cond):
    """Two conditions are the same when they say the same thing, spacing aside."""
    return (re.sub(r"\s+", "", cond["text"]), cond["kind"])


UI_NAME = re.compile(r"(CfgUi|Ui|UiSection|Option|Options|Menu|Dropdown|Section|Cfg)$")


class Graph:
    """The call graph of one scope, and the conditions it can prove."""

    def __init__(self, units, scopeFiles, dropUiCallers=False):
        self.defined = set()
        self.addrTaken = set()
        for unit in units:
            self.defined.update(unit["funcs"])
            self.addrTaken.update(unit["addrTaken"])
        self.external = self.addrTaken & self.defined
        self.calls = []
        for unit in units:
            if unit["file"] not in scopeFiles:
                continue
            for call in unit["calls"]:
                if dropUiCallers and call["caller"] and UI_NAME.search(call["caller"]):
                    continue          # drawing a control is not drawing the track
                self.calls.append(call)
        self.callsFrom = collections.defaultdict(set)
        for call in self.calls:
            if call["caller"]:
                self.callsFrom[call["caller"]].add(call["callee"])
        self.callsTo = collections.defaultdict(list)
        self.cache = {}
        self.reachable = set()

    def restrictToReachable(self):
        """Keep only call sites this side of the browser can actually execute.

        Done after reachFrom, because a call sitting in a shared file is only a
        real call site for this scope when something in the scope reaches it.
        """
        self.callsTo = collections.defaultdict(list)
        for call in self.calls:
            if call["caller"] is None or call["caller"] in self.reachable:
                self.callsTo[call["callee"]].append(call)
        self.cache = {}

    def reachFrom(self, entries):
        """Functions reachable from a scope's own entry points.

        A read in a shared file belongs to a scope only when the drawing side,
        or the config side, can actually get to it.  hicUiGetArcLimit lives in
        hicUi.c and is reached from both; the cfgUi functions beside it are
        reached only from the config page.
        """
        seen, todo = set(), list(entries)
        while todo:
            func = todo.pop()
            if func in seen:
                continue
            seen.add(func)
            todo.extend(self.callsFrom.get(func, ()))
        self.reachable = seen
        return seen

    def necessary(self, func, stack=None):
        """Conditions true on every path into func.  None when that is unknowable."""
        if func in self.cache:
            return self.cache[func]
        stack = stack or set()
        if func in stack or func in self.external or func not in self.defined:
            return None
        sites = self.callsTo.get(func)
        if not sites:
            self.cache[func] = None
            return None
        self.cache[func] = None                  # break cycles while we recurse
        stack.add(func)
        common = None
        for site in sites:
            here = {condKey(c): c for c in site["conds"]}
            up = self.necessary(site["caller"], stack)
            if up:
                here.update({condKey(c): c for c in up})
            if common is None:
                common = dict(here)
            else:
                common = {k: v for k, v in common.items() if k in here}
            if not common:
                break
        stack.discard(func)
        self.cache[func] = list(common.values()) if common else []
        return self.cache[func]


def harvest(kentSrc):
    """Scan the tree and return one record per setting read, per scope."""
    patterns = sorted({p for pats in SCOPES.values() for p in pats} | set(SHARED))
    files = []
    for pattern in patterns:
        files += sorted(glob.glob(os.path.join(kentSrc, pattern)))
    files = sorted(set(files))
    if len(files) < 50:
        raise SystemExit("only %d source files found under %s; is KENT_SRC right?"
                         % (len(files), kentSrc))

    defines = loadDefines(glob.glob(os.path.join(kentSrc, "hg/inc/*.h"))
                          + glob.glob(os.path.join(kentSrc, "inc/*.h"))
                          + glob.glob(os.path.join(kentSrc, "hg/hgTracks/*.h")))
    units = [scanFile(f, kentSrc, defines) for f in files]
    byFile = {u["file"]: u for u in units}

    out = []
    for scope, pats in SCOPES.items():
        ownFiles = set()
        for pattern in pats:
            for f in glob.glob(os.path.join(kentSrc, pattern)):
                ownFiles.add(os.path.relpath(f, kentSrc))
        if scope == "render":
            ownFiles -= set(NOT_RENDER)
        scopeFiles = set(ownFiles)
        for pattern in SHARED:
            for f in glob.glob(os.path.join(kentSrc, pattern)):
                scopeFiles.add(os.path.relpath(f, kentSrc))
        graph = Graph(units, scopeFiles, dropUiCallers=(scope == "render"))

        entries = set()
        for rel in ownFiles:
            if scope == "render" and rel in NOT_RENDER:
                continue
            for func in byFile.get(rel, {}).get("funcs", []):
                if scope == "render" and UI_NAME.search(func):
                    continue          # the config page, reached from hgTracks
                entries.add(func)
        if scope == "config":
            for unit in units:
                for func in unit["funcs"]:
                    if UI_NAME.search(func):
                        entries.add(func)
        reachable = graph.reachFrom(entries)
        graph.restrictToReachable()

        for rel in sorted(scopeFiles):
            unit = byFile.get(rel)
            if not unit:
                continue
            for read in unit["reads"]:
                if scope == "render" and rel in NOT_RENDER:
                    continue
                if rel not in ownFiles and read["func"] not in reachable:
                    continue          # this side of the browser never gets here
                if scope == "render" and read["func"] and UI_NAME.search(read["func"]):
                    continue
                up = graph.necessary(read["func"])
                record = dict(read)
                record["scope"] = scope
                record["callerConds"] = up or []
                record["callerUnknown"] = up is None
                out.append(record)
    return out


def isNoise(cond):
    """Housekeeping, not a condition on the setting.

    A loop header says the code is walking a list, not that the setting only
    applies sometimes.  A test of a variable that holds another setting is never
    noise, however much it looks like a null check: scoreMinStr != NULL is the
    scoreMin gate.
    """
    if cond["kind"] in ("for", "while"):
        return True
    if cond.get("derivedFrom"):
        return False
    return bool(NOISE.match(cond["text"]))


def realConds(read):
    """The read's conditions with the housekeeping tests dropped."""
    seen, out = set(), []
    for cond in read["conds"] + read["callerConds"]:
        if isNoise(cond):
            continue
        key = condKey(cond)
        if key in seen:
            continue
        seen.add(key)
        out.append(cond)
    return out


def main():
    kentSrc = os.environ.get("KENT_SRC") or os.path.abspath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
    reads = harvest(kentSrc)
    if len(sys.argv) > 1 and sys.argv[1] not in ("-", "--stdout"):
        with open(sys.argv[1], "w") as f:
            json.dump(reads, f, indent=1)
        print("wrote %s" % sys.argv[1])
    byScope = collections.Counter(r["scope"] for r in reads)
    tdbReads = [r for r in reads if r["tdb"]]
    print("read sites      %d  (%s)" % (len(reads), dict(byScope)))
    print("  track settings %d, distinct %d" % (len(tdbReads), len({r["name"] for r in tdbReads})))
    print("  page cart vars %d, distinct %d"
          % (len(reads) - len(tdbReads), len({r["name"] for r in reads if not r["tdb"]})))
    for scope in SCOPES:
        rs = [r for r in reads if r["scope"] == scope and r["tdb"]]
        print("%-7s guarded at the read %4d   guarded at a caller %4d   any %4d of %d"
              % (scope,
                 sum(1 for r in rs if [c for c in r["conds"] if not isNoise(c)]),
                 sum(1 for r in rs if [c for c in r["callerConds"] if not isNoise(c)]),
                 sum(1 for r in rs if realConds(r)), len(rs)))


if __name__ == "__main__":
    main()
