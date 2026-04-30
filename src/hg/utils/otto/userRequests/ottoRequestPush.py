#!/usr/bin/env python3
"""
ottoRequestPush.py - group fromDb/toDb identifiers from pending push
requests (status=5) by clade.

Output: dict[clade] -> sorted list of assembly identifiers, where each
identifier is "<gcAccession>_<asmName>" for GenArk accessions, or the
plain UCSC db name for native dbs.
"""

import fcntl
import os
import re
import subprocess
import sys
from collections import defaultdict

scriptDir = os.path.dirname(os.path.abspath(__file__))
cladeTsv = os.path.join(scriptDir, "dbDb.name.clade.tsv")
lockPath = os.path.join(scriptDir, "ottoRequestPush.lock")
gcPattern = re.compile(r"^GC[AF]_")


def acquireSingletonLock():
    """Ensure only one instance of this script runs at a time.  Holds an
    exclusive flock on lockPath for the lifetime of the process; the
    kernel releases it on exit (including crash / kill -9), so no stale
    lock cleanup is needed.  Returns the open file handle, which the
    caller must keep alive."""
    fh = open(lockPath, "w")
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        sys.exit(0)
    return fh

def hgsql(query, db="hgcentraltest"):
    """Run hgsql -N -B and return rows as list of tuples (tab-split)."""
    out = subprocess.run(
        ["hgsql", "-N", "-B", "-e", query, db],
        check=True, capture_output=True, text=True,
    ).stdout
    return [tuple(line.split("\t")) for line in out.splitlines() if line]


def loadDbDbClades():
    """Read dbDb.name.clade.tsv -> {dbName: clade}."""
    result = {}
    with open(cladeTsv) as fh:
        for line in fh:
            if line.startswith("#") or not line.strip():
                continue
            name, clade = line.rstrip("\n").split("\t")[:2]
            result[name] = clade
    return result


def pendingRequests():
    """Status=5 liftOver requests as [(id, fromDb, toDb), ...]."""
    rows = hgsql(
        "SELECT id, fromDb, toDb FROM ottoRequest "
        "WHERE status = 5 AND requestType = 'liftOver';"
    )
    return [(int(r[0]), r[1], r[2]) for r in rows]


def markComplete(reqIds):
    """Set status=6 on the given ottoRequest ids."""
    if not reqIds:
        return
    idList = ",".join(str(i) for i in sorted(reqIds))
    hgsql("UPDATE ottoRequest SET status = 6 WHERE id IN (%s);" % idList)
    print("# marked status=6: %s" % idList, file=sys.stderr)


def lookupGenark(accessions):
    """Bulk-lookup GenArk accessions -> {acc: (asmName, clade)}."""
    if not accessions:
        return {}
    quoted = ",".join("'%s'" % a for a in sorted(accessions))
    rows = hgsql(
        "SELECT gcAccession, asmName, clade FROM genark "
        "WHERE gcAccession IN (%s);" % quoted
    )
    return {acc: (asmName, clade) for acc, asmName, clade in rows}


def groupByClade(dbs, dbDbClades, genarkInfo):
    """Build {clade: [assemblyId, ...]}."""
    grouped = defaultdict(set)
    for db in dbs:
        if gcPattern.match(db):
            info = genarkInfo.get(db)
            if info is None:
                print("WARNING: %s not in genark table" % db, file=sys.stderr)
                continue
            asmName, clade = info
            grouped[clade].add("%s_%s" % (db, asmName))
        else:
            clade = dbDbClades.get(db)
            if clade is None:
                print("WARNING: %s not in %s" % (db, cladeTsv), file=sys.stderr)
                continue
            grouped[clade].add(db)
    return {clade: sorted(ids) for clade, ids in grouped.items()}


def writeCladeTsv(clade, asmIds):
    """Filter <clade>.orderList.tsv down to lines matching any asmId and
    write the result to tsv.otto in the same directory.  Mirrors:
        cd ~/kent/src/hg/makeDb/doc/<clade>AsmHub
        egrep '<id1>|<id2>|...' <clade>.orderList.tsv > tsv.otto
    Only GenArk identifiers are used UCSC native dbs are not in the
    AsmHub orderList files.

    Returns cladeDir on success (so the caller can chain the make
    sequence), or None if there is nothing to do for this clade.
    """
    genarkIds = [a for a in asmIds if gcPattern.match(a)]
    if not genarkIds:
        return None
    cladeDir = os.path.expanduser(
        "~/kent/src/hg/makeDb/doc/%sAsmHub" % clade)
    orderList = os.path.join(cladeDir, "%s.orderList.tsv" % clade)
    outPath = os.path.join(cladeDir, "tsv.otto")
    if not os.path.isfile(orderList):
        print("WARNING: missing %s" % orderList, file=sys.stderr)
        return None
    matched = []
    with open(orderList) as fh:
        for line in fh:
            if any(asmId in line for asmId in genarkIds):
                matched.append(line)
    if not matched:
        print("WARNING: no matches in %s" % orderList, file=sys.stderr)
        return None
    with open(outPath, "w") as fh:
        fh.writelines(matched)
    print("# wrote %d line(s) to %s" % (len(matched), outPath),
          file=sys.stderr)
    return cladeDir


# Sequence of make commands run in the clade AsmHub directory after
# tsv.otto is written.  Stops on the first failure.
makeChainCommands = [
    "time (make symLinks orderList=tsv.otto) >> dbg 2>&1",
    "time (make mkGenomes orderList=tsv.otto) >> dbg 2>&1",
    "time (make symLinks orderList=tsv.otto) >> dbg 2>&1",
    "time (make verifyTestDownload orderList=tsv.otto) >> test.down.log 2>&1",
    "time (make sendDownload orderList=tsv.otto) >> send.down.log 2>&1",
    "time (make verifyDownload orderList=tsv.otto) >> verify.down.log 2>&1",
]


def runMakeChain(cladeDir):
    """Run the post-tsv.otto make sequence in cladeDir.  Uses bash so
    'time (...)' (a builtin on a subshell) and '>>' / '2>&1' work as
    written.  Returns True on success, False if any step fails (the
    chain stops at the first failure)."""
    for cmd in makeChainCommands:
        print("# [%s] %s" % (cladeDir, cmd), file=sys.stderr)
        result = subprocess.run(
            cmd, shell=True, executable="/bin/bash", cwd=cladeDir,
        )
        if result.returncode != 0:
            print("# ERROR: exit %d from: %s -- stopping chain"
                  % (result.returncode, cmd), file=sys.stderr)
            return False
    return True


def main():
    lockFh = acquireSingletonLock()  # noqa: F841 -- keep ref alive
    requests = pendingRequests()
    if not requests:
        return
    dbs = set()
    for _, fromDb, toDb in requests:
        dbs.update((fromDb, toDb))
    accessions = {db for db in dbs if gcPattern.match(db)}
    dbDbClades = loadDbDbClades()
    genarkInfo = lookupGenark(accessions)
    grouped = groupByClade(dbs, dbDbClades, genarkInfo)

    def cladeOf(db):
        if gcPattern.match(db):
            info = genarkInfo.get(db)
            return info[1] if info else None
        return dbDbClades.get(db)

    succeededClades = set()
    failedClades = set()
    for clade in sorted(grouped):
        print("%s:" % clade)
        for asmId in grouped[clade]:
            print("  %s" % asmId)
        cladeDir = writeCladeTsv(clade, grouped[clade])
        if cladeDir is None:
            continue
        if runMakeChain(cladeDir):
            succeededClades.add(clade)
        else:
            failedClades.add(clade)

    completedIds = []
    failedRequests = []
    for reqId, fromDb, toDb in requests:
        if not (gcPattern.match(fromDb) and gcPattern.match(toDb)):
            continue                          # mixed or UCSC, not pushed here
        fromClade = cladeOf(fromDb)
        toClade = cladeOf(toDb)
        if fromClade is None or toClade is None:
            continue                          # missing clade info; be safe
        clades = {fromClade, toClade}
        if clades.issubset(succeededClades):
            completedIds.append(reqId)
        elif clades & failedClades:
            failedRequests.append(
                (reqId, fromDb, toDb, sorted(clades & failedClades)))

    markComplete(completedIds)

    if failedRequests:
        print("# the following request(s) stay at status=5 due to failed "
              "clade pushes:", file=sys.stderr)
        for reqId, fromDb, toDb, badClades in failedRequests:
            print("#   id=%d %s -> %s (failed clade(s): %s)"
                  % (reqId, fromDb, toDb, ", ".join(badClades)),
                  file=sys.stderr)


if __name__ == "__main__":
    main()
