#!/usr/bin/env python3
"""
ottoBuildGenArkHub.py - given a list of GenArk accession identifiers,
run each asm's doTrackDb.bash (the way ottoRequestWatch.sh does),
group the survivors by clade, write tsv.otto in the matching
cladeAsmHub source directory (~/kent/src/hg/makeDb/doc/<clade>AsmHub),
and run the genArkMakeCommands sequence there to (re)construct the
assembly hub files.

Usage:
  ottoBuildGenArkHub.py [-f FILE] [accession ...]

Accessions may be supplied as positional args and/or via -f FILE
(one or more per line, whitespace-separated; '#' comments allowed).
Each entry may be either the bare 'GCA_000001405.15' or the full
'GCA_000001405.15_GRCh38' - any trailing _<asmName> is stripped and
the asmName is looked up in the hgcentraltest.genark table along
with the clade.

This script shares the ottoRequestPush.lock with the live cron, so a
manual run will exit non-zero if the cron is currently building.
"""

import argparse
import os
import sys

import ottoLib

scriptDir = os.path.dirname(os.path.abspath(__file__))
lockPath = os.path.join(scriptDir, "ottoRequestPush.lock")
# match ottoRequestWatch.sh's log location for doTrackDb output
doTdbLog = os.path.join(scriptDir, "doTdb.log")


def parseAccession(arg):
    """Strip an optional _<asmName> suffix and return the bare GenArk
    accession, or None if arg doesn't look like one.  Accepts both
    'GCA_000001405.15' and 'GCA_000001405.15_GRCh38'."""
    if not ottoLib.gcPattern.match(arg):
        return None
    parts = arg.split("_", 2)
    return "_".join(parts[:2])


def readAccessions(args, parser):
    """Collect accessions from positional args + -f FILE.  De-dupes
    while preserving first-seen order.  Calls parser.error() if no
    valid accessions were supplied."""
    rawInputs = list(args.accession)
    if args.file:
        with open(args.file) as fh:
            for line in fh:
                line = line.split("#", 1)[0]
                rawInputs.extend(line.split())
    seen = set()
    accessions = []
    for arg in rawInputs:
        bare = parseAccession(arg)
        if bare is None:
            print("WARNING: not a GenArk accession: %s" % arg, file=sys.stderr)
            continue
        if bare in seen:
            continue
        seen.add(bare)
        accessions.append(bare)
    if not accessions:
        parser.error("no GenArk accessions supplied")
    return accessions


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "accession", nargs="*",
        help="GenArk accession (GC[AF]_<digits.version>), optionally "
             "with a trailing _<asmName> that will be stripped",
    )
    parser.add_argument(
        "-f", "--file",
        help="file with one or more accessions "
             "(whitespace-separated; '#' starts a comment)",
    )
    args = parser.parse_args()

    accessions = readAccessions(args, parser)

    lockFh = ottoLib.acquireSingletonLock(lockPath, exitOnLocked=False)
    if lockFh is None:
        print("ERROR: another otto build is already running (%s held)"
              % lockPath, file=sys.stderr)
        sys.exit(1)
    # keep ref alive for the lifetime of the process
    _ = lockFh

    genarkInfo = ottoLib.lookupGenark(accessions)
    for acc in accessions:
        if acc not in genarkInfo:
            print("WARNING: %s not in genark table - skipping"
                  % acc, file=sys.stderr)

    grouped = ottoLib.groupByClade(set(genarkInfo.keys()), {}, genarkInfo)
    if not grouped:
        print("ERROR: nothing to do - no accessions resolved to a clade",
              file=sys.stderr)
        sys.exit(1)

    # doTrackDb runs first: it refreshes per-asm trackDb stanzas that the
    # downstream make chain then bakes into the hub files.  An asm whose
    # doTrackDb fails is dropped from this build pass (the make chain
    # would otherwise bake stale or broken trackDb into the hub files);
    # a clade whose every asm fails doTrackDb is dropped entirely.
    doTdbFailures = []
    for clade in sorted(grouped):
        survivors = []
        for asmId in grouped[clade]:
            if ottoLib.runDoTrackDb(asmId, logPath=doTdbLog):
                survivors.append(asmId)
            else:
                doTdbFailures.append(asmId)
                print("# WARNING: doTrackDb failed for %s - dropping "
                      "from this build pass" % asmId, file=sys.stderr)
        grouped[clade] = survivors
    grouped = {c: ids for c, ids in grouped.items() if ids}

    failedClades = set()
    builtClades = set()
    for clade in sorted(grouped):
        cladeDir = ottoLib.writeCladeTsv(clade, grouped[clade])
        if cladeDir is None:
            continue
        if ottoLib.runGenArkMake(cladeDir):
            builtClades.add(clade)
        else:
            failedClades.add(clade)

    if failedClades:
        print("# build failures in clade(s): %s"
              % ", ".join(sorted(failedClades)), file=sys.stderr)
    if doTdbFailures:
        print("# doTrackDb.bash failed for: %s"
              % ", ".join(doTdbFailures), file=sys.stderr)

    if failedClades or doTdbFailures:
        sys.exit(1)


if __name__ == "__main__":
    main()
