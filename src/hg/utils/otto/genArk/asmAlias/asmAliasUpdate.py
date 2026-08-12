#!/usr/bin/env python3
"""
asmAliasUpdate.py

Prepare a complete replacement tab separated two column (alias, browser)
dump of the hgcentraltest 'asmAlias' table: every existing row is carried
forward untouched, EXCEPT:
  - rows on a GenBank (GCA) browser that has since been superseded by an
    equivalent live RefSeq (GCF) browser -- those are dropped and
    replaced with equivalent rows pointing at the new RefSeq browser
  - a row whose alias's genark data now resolves to a newer dot version
    of the *same* GCA/GCF accession root (see isSameAssemblyNewerVersion)
    -- these are repointed ('UPDATE:') to the newer version.  This is
    never done when the existing browser is a plain UCSC database name
    (e.g. 'hg38', 'dm6') rather than a GCA_/GCF_ accession -- those are
    always left alone, and any other kind of mismatch is only reported
    ('ERROR:'), never silently changed
New rows discovered from the genark data are appended; a candidate alias
that is nothing but digits and dots (e.g. '1.0', '26') -- a bare version
number, never a useful alias -- is never generated in the first place.

As a final pass over the complete row set (existing + new), any two alias
spellings that differ only by case (e.g. 'FF3'/'Ff3') are resolved, since
asmAlias lookups are case-insensitive and such pairs can never coexist as
distinct primary keys: if they point to the same browser, one spelling is
kept and the duplicate silently dropped ('NOTE:'); if they point to
different browsers, there is no way to know which is right, so the whole
group is dropped and reported ('ERROR:').

The intent is that this file becomes the entire new table content (e.g.
via a truncate-and-reload), not a delta to apply by hand.  The script
only reports (to stderr) other problems it finds with the existing data
-- orphaned browsers, alias collisions, apparent name changes -- for a
human to decide what to do about; it does not fix those itself.

Data sources, all read live and kept in memory -- no intermediate files:
  - hgcentraltest.asmAlias                    (via hgsql)
  - genark.assemblySummaryGenbank             (via hgsql)
  - genark.assemblySummaryGenbankHistorical   (via hgsql)
  - genark.assemblySummaryRefseq              (via hgsql)
  - genark.assemblySummaryRefseqHistorical    (via hgsql)
  - https://hgdownload.gi.ucsc.edu/hubs/UCSC_GI.assemblyHubList.txt
      column 1 is the set of accessions that actually have a working
      GenArk hub (browser) instance today.  asmAlias.browser must
      reference something that actually exists, so only accessions
      found in this list can become a 'browser' value here.

Every live-hub accession is first assigned a "target" browser value
(computeBrowserTargets): normally an accession targets itself, but a
GenBank (GCA) accession whose paired RefSeq (GCF) accession also has a
live hub is considered superseded -- RefSeq is always preferred, so its
target is redirected to that GCF instead.  All of the accession's own
alias candidates, including its bare accession text, then attach to its
target rather than to itself; this is what produces rows like
alias=GCA_xxx browser=GCF_xxx once a GCF hub appears for an assembly
that used to only have a GCA hub.

For every live-hub accession, alias candidates derived from its own data
are considered in this order of preference (all pointed at its target):
  - the ftpPath-derived name: the basename of ftpPath (an
    accession_asmName style directory name) with the leading
    "accession_" stripped off -- this is the best name, it never has
    spaces
  - the submitter supplied asmName, but only when it contains no spaces
    (asmName is not usable as a SQL/URL style alias when it has spaces)
    and only when it differs from the ftpPath-derived name (underscore
    vs. hyphen style differences between the two are common and fine --
    both get added in that case)
  - the full ftpPath basename itself (accession_asmName, un-stripped) --
    technically redundant with the accession + the stripped name above,
    but it completes the set of identifiers seen in the wild and is a
    harmless alias to have on file
  - if this accession's target is not itself (a superseded GCA), its own
    bare accession text
  - the GenBank<->RefSeq paired accession (gbrsPairedAsm), but only when
    that paired accession does NOT have a live hub at all (the superseded
    case above is handled separately, by the point above)

When an alias candidate is claimed by more than one target browser (e.g.
two dot-versions of the same accession both produced the identical
name), the conflict is resolved rather than reported as an error:
  - a RefSeq (GCF) browser is always preferred over a GenBank (GCA) one
  - among targets sharing the same prefix and numeric id, the higher dot
    version wins
Only a genuinely unresolvable case (different underlying numeric ids
coincidentally producing the same alias text) is reported as an error
and skipped.

Output (mandatory; following kent convention, the names 'stdout' and
'stderr' mean the corresponding stream rather than a literal file of
that name):
  - the complete new alias/browser table content -> -o/--outFile, no
    header line
  - problem/status reports always go to the real stderr

Usage:
    buildAsmAliasUpdate.py -o outFile
"""

import sys
import re
import argparse
import subprocess
import urllib.request

hgsqlCmd = "hgsql"
genarkDb = "genark"
centralDb = "hgcentraltest"
hubListUrl = "https://hgdownload.gi.ucsc.edu/hubs/UCSC_GI.assemblyHubList.txt"

genarkTables = [
    "assemblySummaryGenbank",
    "assemblySummaryGenbankHistorical",
    "assemblySummaryRefseq",
    "assemblySummaryRefseqHistorical",
]

# values NCBI/mysql use to mean "no value"
emptyValues = {"", "na", "NULL", "\\N"}


def isEmpty(value):
    return value is None or value in emptyValues


numericAliasRe = re.compile(r'^[0-9.]+$')


def isNumericOnlyAlias(alias):
    """True for alias text made up only of digits and dots (e.g. '1.0',
    '2.0', '26') -- a bare version number, not a useful alias."""
    return bool(numericAliasRe.match(alias))


def hgsqlRows(db, sql):
    """Run 'hgsql db -N -e sql' and return a list of tab-split field lists."""
    cmd = [hgsqlCmd, db, "-N", "-e", sql]
    result = subprocess.run(cmd, capture_output=True, check=True)
    result.stdout = result.stdout.decode("utf-8", errors="replace")
    rows = []
    for line in result.stdout.rstrip("\n").split("\n"):
        if line == "":
            continue
        rows.append(line.split("\t"))
    return rows


def fetchHubList(url):
    """Return the set of accessions (column 1) that have a working GenArk hub."""
    hubAccessions = set()
    with urllib.request.urlopen(url) as resp:
        rawBytes = resp.read()
    # the common-name column can carry non-UTF-8 bytes (e.g. Latin-1); only
    # column 0 (the accession) is used here, so tolerate/replace the rest
    text = rawBytes.decode("utf-8", errors="replace")
    for rawLine in text.split("\n"):
        line = rawLine.rstrip("\n")
        if line == "" or line.startswith("#"):
            continue
        acc = line.split("\t")[0]
        hubAccessions.add(acc)
    return hubAccessions


def fetchGenarkData():
    """Return dict: assemblyAccession -> (asmName, gbrsPairedAsm, ftpPath, table)."""
    genarkData = {}
    for table in genarkTables:
        sql = ("SELECT assemblyAccession, asmName, gbrsPairedAsm, ftpPath "
               "FROM %s" % table)
        for row in hgsqlRows(genarkDb, sql):
            if len(row) != 4:
                continue
            accession, asmName, gbrsPairedAsm, ftpPath = row
            if accession in genarkData:
                prior = genarkData[accession]
                if prior[0:3] != (asmName, gbrsPairedAsm, ftpPath):
                    sys.stderr.write(
                        "WARNING: %s appears in both %s and %s with "
                        "differing data, keeping first seen\n"
                        % (accession, prior[3], table))
                continue
            genarkData[accession] = (asmName, gbrsPairedAsm, ftpPath, table)
    return genarkData


def fetchExistingAsmAlias():
    """Return (aliasToBrowser dict, browserToAliases dict) from hgcentraltest."""
    aliasToBrowser = {}
    browserToAliases = {}
    for alias, browser in hgsqlRows(centralDb, "SELECT alias, browser FROM asmAlias"):
        aliasToBrowser[alias] = browser
        browserToAliases.setdefault(browser, set()).add(alias)
    return aliasToBrowser, browserToAliases


def ftpPathBasename(ftpPath):
    if isEmpty(ftpPath):
        return ""
    return ftpPath.rstrip("/").split("/")[-1]


def ftpDerivedName(accession, ftpPath):
    """The ftpPath basename with the leading 'accession_' prefix removed,
    e.g. ftpPath .../GCA_012345678.1_someAsmName -> 'someAsmName'."""
    base = ftpPathBasename(ftpPath)
    if not base:
        return ""
    prefix = accession + "_"
    if base.startswith(prefix):
        return base[len(prefix):]
    if base == accession:
        return ""
    # unexpected shape, but still usable as-is
    return base


def buildCandidates(accession, target, asmName, gbrsPairedAsm, ftpPath, hubAccessions):
    """Alias candidates derived from 'accession's own data that should
    point at 'target' -- normally target == accession, but a GenBank
    accession superseded by a live paired RefSeq hub redirects target to
    that RefSeq accession (see computeBrowserTargets)."""
    candidates = set()

    bestName = ftpDerivedName(accession, ftpPath)
    if bestName and bestName != target:
        candidates.add(bestName)

    # asmName is only usable as an alias when it has no spaces; it's worth
    # adding in addition to bestName when it differs (underscore/hyphen
    # style differences between the two are common and both are useful)
    if (not isEmpty(asmName) and asmName != target and " " not in asmName
            and asmName != bestName):
        candidates.add(asmName)

    # the full un-stripped ftpPath basename (accession_asmName) -- mostly
    # redundant, but it's a complete identifier and a harmless extra alias
    fullIdentifier = ftpPathBasename(ftpPath)
    if fullIdentifier and fullIdentifier != target:
        candidates.add(fullIdentifier)

    if target != accession:
        # this accession has been superseded by its paired browser (see
        # computeBrowserTargets); its own bare accession becomes an alias
        # to the new target, e.g. alias=GCA_xxx browser=GCF_xxx
        candidates.add(accession)

    if not isEmpty(gbrsPairedAsm) and gbrsPairedAsm != target:
        # only claim the paired accession as an alias if it does NOT have
        # its own separate hub at all -- otherwise it's handled either by
        # its own row directly, or via the "superseded" branch above
        if gbrsPairedAsm not in hubAccessions:
            candidates.add(gbrsPairedAsm)

    # a bare version number (e.g. '1.0', '26') is never a useful alias
    return {c for c in candidates if not isNumericOnlyAlias(c)}


def computeBrowserTargets(genarkData, hubAccessions):
    """For every live-hub accession, decide which browser value its own
    alias candidates should point at: itself, unless it is a GenBank
    (GCA) accession whose paired RefSeq (GCF) accession also has a live
    hub -- in that case it is superseded and everything about it
    redirects to the paired GCF (GCF is always preferred over GCA)."""
    targets = {}
    for accession, (asmName, gbrsPairedAsm, ftpPath, table) in genarkData.items():
        if accession not in hubAccessions:
            continue
        if (accession.startswith("GCA_") and not isEmpty(gbrsPairedAsm)
                and gbrsPairedAsm in hubAccessions):
            targets[accession] = gbrsPairedAsm
        else:
            targets[accession] = accession
    return targets


accessionRe = re.compile(r'^(GCA|GCF)_(\d+)\.(\d+)$')


def parseAccession(accession):
    """Return (prefix, numericId, version:int); tolerates unexpected shapes."""
    m = accessionRe.match(accession)
    if m:
        return m.group(1), m.group(2), int(m.group(3))
    return accession[:3], accession, 0


def isSameAssemblyNewerVersion(oldBrowser, newBrowser):
    """True if oldBrowser and newBrowser are the same GCA/GCF accession
    root (same prefix + numeric id) with newBrowser a higher dot version
    -- a safe version-bump update, not an unrelated conflict.  False if
    either side isn't a GCA_/GCF_ style accession at all (e.g. a plain
    UCSC database name like 'hg38' or 'dm6') -- those must never be
    silently repointed."""
    oldMatch = accessionRe.match(oldBrowser)
    newMatch = accessionRe.match(newBrowser)
    if not oldMatch or not newMatch:
        return False
    oldPrefix, oldId, oldVersion = oldMatch.group(1), oldMatch.group(2), int(oldMatch.group(3))
    newPrefix, newId, newVersion = newMatch.group(1), newMatch.group(2), int(newMatch.group(3))
    return oldPrefix == newPrefix and oldId == newId and newVersion > oldVersion


def pickWinner(accessions):
    """Given a set of live-hub accessions all wanting the same alias text,
    resolve to a single winner: RefSeq (GCF) beats GenBank (GCA), and
    among accessions sharing a prefix+numeric id the higher dot version
    wins.  Returns (winner, discarded), or (None, accessions) if the
    conflict can't be resolved by those rules."""
    accessions = sorted(accessions)
    if len(accessions) == 1:
        return accessions[0], []

    parsed = {a: parseAccession(a) for a in accessions}
    gcf = [a for a in accessions if parsed[a][0] == "GCF"]
    gca = [a for a in accessions if parsed[a][0] == "GCA"]
    pool = gcf if gcf else gca
    discarded = [a for a in accessions if a not in pool]

    numericIds = {parsed[a][1] for a in pool}
    if len(numericIds) > 1:
        # different underlying assemblies coincidentally produced the
        # same alias text -- no rule resolves this
        return None, accessions

    winner = max(pool, key=lambda a: parsed[a][2])
    discarded += [a for a in pool if a != winner]
    return winner, discarded


def reportExistingRowProblems(aliasToBrowser, hubAccessions, genarkData):
    for alias, browser in aliasToBrowser.items():
        if browser.startswith("GCA_") or browser.startswith("GCF_"):
            if browser not in hubAccessions:
                sys.stderr.write(
                    "ERROR: existing asmAlias row alias=%s browser=%s -- "
                    "browser is not in the current GenArk hub list\n"
                    % (alias, browser))
            elif browser not in genarkData:
                sys.stderr.write(
                    "ERROR: existing asmAlias row alias=%s browser=%s -- "
                    "browser accession not found in any genark summary "
                    "table\n" % (alias, browser))


def findObsoleteGcaBrowsers(browserTargets, browserToAliases):
    """Return dict: obsolete GCA (GenBank) browser -> its equivalent live
    GCF (RefSeq) browser, restricted to browsers that actually have
    existing asmAlias rows on file today -- those rows get dropped from
    the carried-forward set since their replacements land on a different
    browser value."""
    obsolete = {}
    for browser in browserToAliases:
        target = browserTargets.get(browser)
        if target and target != browser:
            obsolete[browser] = target
    return obsolete


def normalizeNameForCompare(name):
    """'-' and '_' are used interchangeably in these names; ignore the
    difference when deciding whether two names are really the same."""
    return name.replace("-", "_")


def isSameNameDifferentFormat(accession, bestName, priorAliases):
    """True if bestName is just a reformatted version of an alias already
    on file for this browser -- e.g. the old full ftpPath-basename-style
    alias 'GCA_000182895.1_CC3' vs. the new prefix-stripped 'CC3', or an
    underscore/hyphen variant -- rather than an actual name change."""
    normBest = normalizeNameForCompare(bestName)
    prefix = accession + "_"
    for prior in priorAliases:
        candidate = prior[len(prefix):] if prior.startswith(prefix) else prior
        if normalizeNameForCompare(candidate) == normBest:
            return True
    return False


def buildNewRows(genarkData, hubAccessions, browserTargets, aliasToBrowser,
                  browserToAliases):
    # first pass: gather every alias candidate text -> set of target
    # browsers that want it, and report apparent name changes along the way
    aliasCandidates = {}   # alias -> set of target browsers
    reportedNameChecks = set()   # (target, bestName) already logged once

    for accession, (asmName, gbrsPairedAsm, ftpPath, table) in genarkData.items():
        target = browserTargets.get(accession)
        if target is None:
            continue  # no live browser instance, can't be used as 'browser'

        bestName = ftpDerivedName(accession, ftpPath)
        priorAliases = browserToAliases.get(target, set())
        if (priorAliases and bestName and bestName not in priorAliases
                and (target, bestName) not in reportedNameChecks):
            reportedNameChecks.add((target, bestName))
            if isSameNameDifferentFormat(accession, bestName, priorAliases):
                sys.stderr.write(
                    "NEW ALIAS: browser=%s -- '%s' is just a reformatted "
                    "version of existing alias(es) %s, adding it as an "
                    "additional alias, not a name change\n"
                    % (target, bestName, sorted(priorAliases)))
            else:
                sys.stderr.write(
                    "NOTE: possible name change for browser=%s -- current "
                    "name='%s' not among existing aliases %s\n"
                    % (target, bestName, sorted(priorAliases)))

        for alias in buildCandidates(accession, target, asmName, gbrsPairedAsm,
                                      ftpPath, hubAccessions):
            aliasCandidates.setdefault(alias, set()).add(target)

    # second pass: resolve each alias to a single winning browser, then
    # reconcile against what's already in the table
    newRows = []
    for alias, accessions in aliasCandidates.items():
        winner, discarded = pickWinner(accessions)
        if winner is None:
            sys.stderr.write(
                "ERROR: alias=%s is claimed by unrelated accessions %s, "
                "not resolvable by version/RefSeq preference, skipping\n"
                % (alias, sorted(accessions)))
            continue
        if discarded:
            sys.stderr.write(
                "NOTE: alias=%s claimed by %s, preferring browser=%s\n"
                % (alias, sorted(accessions), winner))

        if alias in aliasToBrowser:
            oldBrowser = aliasToBrowser[alias]
            if oldBrowser == winner:
                continue  # already correct, nothing to do
            if isSameAssemblyNewerVersion(oldBrowser, winner):
                # same GCA/GCF accession root, just a newer dot version --
                # safe to repoint; never done when oldBrowser is a plain
                # UCSC database name (isSameAssemblyNewerVersion is False
                # in that case)
                sys.stderr.write(
                    "UPDATE: alias=%s browser %s -> %s (same assembly, "
                    "newer version)\n" % (alias, oldBrowser, winner))
                newRows.append((alias, winner))
                continue
            sys.stderr.write(
                "ERROR: alias=%s already exists in asmAlias pointing "
                "to browser=%s, genark data wants it to point to "
                "browser=%s -- not touching existing row\n"
                % (alias, oldBrowser, winner))
            continue  # a conflict we must not silently overwrite

        newRows.append((alias, winner))

    newRows.sort()
    return newRows


def resolveCaseInsensitiveConflicts(finalDict):
    """asmAlias lookups are case-insensitive, so two alias spellings that
    differ only by case can never coexist as distinct primary keys.
    Group by lowercase form: when every entry in a group points to the
    same browser, it's a harmless duplicate -- keep just one (the
    alphabetically first spelling); when a group points to more than one
    browser, there's no way to know which spelling is right, so drop the
    whole group and report it."""
    byLower = {}
    for alias, browser in finalDict.items():
        byLower.setdefault(alias.lower(), []).append((alias, browser))

    resolved = {}
    for lower, entries in byLower.items():
        if len(entries) == 1:
            alias, browser = entries[0]
            resolved[alias] = browser
            continue

        entries.sort()
        browsers = {b for a, b in entries}
        if len(browsers) == 1:
            keeper, browser = entries[0]
            sys.stderr.write(
                "NOTE: alias(es) %s are case-insensitive duplicates all "
                "pointing to browser=%s, keeping only '%s'\n"
                % ([a for a, b in entries], browser, keeper))
            resolved[keeper] = browser
        else:
            sys.stderr.write(
                "ERROR: alias(es) %s are not case-independent (asmAlias "
                "lookups are case-insensitive) and point to different "
                "browsers %s -- dropping all of them\n"
                % ([a for a, b in entries], sorted(browsers)))
    return resolved


def mustOpen(name):
    """Open name for writing, following kent convention: the names
    'stdout' and 'stderr' mean the corresponding stream rather than a
    literal file of that name.  Returns (handle, shouldClose)."""
    if name == "stdout":
        return sys.stdout, False
    if name == "stderr":
        return sys.stderr, False
    return open(name, "w"), True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--outFile", required=True,
                         help="write the complete replacement asmAlias "
                              "table content here ('stdout'/'stderr' "
                              "allowed)")
    args = parser.parse_args()

    sys.stderr.write("fetching GenArk hub list ...\n")
    hubAccessions = fetchHubList(hubListUrl)
    sys.stderr.write("  %d hub accessions found\n" % len(hubAccessions))

    sys.stderr.write("fetching genark assembly summary tables ...\n")
    genarkData = fetchGenarkData()
    sys.stderr.write("  %d accessions found\n" % len(genarkData))

    sys.stderr.write("fetching existing hgcentraltest.asmAlias ...\n")
    aliasToBrowser, browserToAliases = fetchExistingAsmAlias()
    sys.stderr.write("  %d existing alias rows\n" % len(aliasToBrowser))

    reportExistingRowProblems(aliasToBrowser, hubAccessions, genarkData)

    # decide, once, which browser value every live accession's own alias
    # candidates should point at (itself, or its live paired GCF if it's
    # a superseded GCA) -- this is the single source of truth the rest of
    # the script builds on
    browserTargets = computeBrowserTargets(genarkData, hubAccessions)

    obsoleteGca = findObsoleteGcaBrowsers(browserTargets, browserToAliases)
    for browser in sorted(obsoleteGca):
        sys.stderr.write(
            "OBSOLETE: browser=%s (GenBank) replaced by browser=%s "
            "(RefSeq) -- used by alias(es) %s, dropping those rows and "
            "replacing them\n"
            % (browser, obsoleteGca[browser], sorted(browserToAliases[browser])))

    # existing rows on a retired GCA browser are dropped from the carried-
    # forward set, and must not block their replacement rows from being
    # added pointing at the new GCF browser instead
    survivors = {a: b for a, b in aliasToBrowser.items() if b not in obsoleteGca}

    newRows = buildNewRows(genarkData, hubAccessions, browserTargets,
                            survivors, browserToAliases)

    # merge as a dict, not a concatenation: newRows normally only adds
    # aliases absent from survivors, but a same-assembly version-bump
    # update (see isSameAssemblyNewerVersion) intentionally reuses an
    # alias already in survivors to repoint it -- the dict assignment
    # is what makes that repointing take effect instead of duplicating
    # the alias key
    finalDict = dict(survivors)
    updated = sum(1 for alias, browser in newRows
                  if alias in finalDict and finalDict[alias] != browser)
    for alias, browser in newRows:
        finalDict[alias] = browser

    beforeCaseCheck = len(finalDict)
    finalDict = resolveCaseInsensitiveConflicts(finalDict)
    caseDropped = beforeCaseCheck - len(finalDict)

    finalRows = sorted(finalDict.items())

    outHandle, closeOut = mustOpen(args.outFile)
    for alias, browser in finalRows:
        outHandle.write("%s\t%s\n" % (alias, browser))
    if closeOut:
        outHandle.close()

    sys.stderr.write("%d obsolete GCA browser(s) dropped\n" % len(obsoleteGca))
    sys.stderr.write("%d existing row(s) repointed to a newer version\n" % updated)
    sys.stderr.write("%d new asmAlias rows added\n" % (len(newRows) - updated))
    sys.stderr.write("%d row(s) dropped for case-insensitive conflicts\n" % caseDropped)
    sys.stderr.write("%d total rows in replacement table (was %d)\n"
                      % (len(finalRows), len(aliasToBrowser)))


if __name__ == "__main__":
    main()
