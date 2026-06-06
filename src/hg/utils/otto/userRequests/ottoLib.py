"""
ottoLib.py - shared helpers for the otto userRequests scripts.

Provides clade lookup (via hgsql -> genark and the dbDb.name.clade.tsv
checked in next to this file), the filter that writes tsv.otto into the
matching cladeAsmHub source directory inside the otto kent working tree,
the 'git pull' helper that brings that tree up to date, and the make-
command sequence that (re)builds a GenArk assembly hub.  Also wraps the
per-asm doTrackDb.bash invocation used by ottoRequestWatch.sh.

The otto kent working tree is resolved the same way chainNetTrackDb.pl
resolves it:
    $OTTO_KENT_TREE  (default /hive/data/outside/genark/ottoKent/kent)

Used by:
  ottoRequestPush2.py     (lib-using rewrite of ottoRequestPush.py)
  ottoBuildGenArkHub.py   (manual driver: rebuild hub files for a list
                           of GenArk accessions)
"""

import fcntl
import os
import re
import subprocess
import sys
from collections import defaultdict

libDir = os.path.dirname(os.path.abspath(__file__))
cladeTsv = os.path.join(libDir, "dbDb.name.clade.tsv")
gcPattern = re.compile(r"^GC[AF]_")

# Otto's kent working tree: where cladeAsmHub directories live and where
# the make chain runs.  Matches chainNetTrackDb.pl's $kentTree resolution.
kentTree = os.environ.get(
    "OTTO_KENT_TREE", "/hive/data/outside/genark/ottoKent/kent")


def acquireSingletonLock(lockPath, exitOnLocked=True):
    """Ensure only one instance holding lockPath runs at a time.  Holds
    an exclusive flock for the lifetime of the process; the kernel
    releases it on exit (including crash / kill -9), so no stale-lock
    cleanup is needed.  Returns the open file handle, which the caller
    must keep alive.

    exitOnLocked=True (cron-style): sys.exit(0) when the lock is held.
    exitOnLocked=False (manual-style): return None so the caller can
    print a message and exit non-zero.
    """
    # "a+" opens read+write without truncating (and creates if missing),
    # so a second instance that fails to lock doesn't wipe the running
    # instance's PID from the file before exiting.
    fh = open(lockPath, "a+")
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        if exitOnLocked:
            sys.exit(0)
        return None
    fh.seek(0)
    fh.truncate()
    fh.write("%d\n" % os.getpid())
    fh.flush()
    return fh
    ### FYI: can also see the locking process via: lsof <lockPath>


def gitPullKentTree():
    """Run 'git -C <kentTree> pull' so make commands run against an
    up-to-date checkout (mirrors the first thing chainNetTrackDb.pl
    does after sanity-checking $kentTree).  Returns True on success,
    False otherwise (with the error printed to stderr).  Untracked
    files such as the regenerated tsv.otto are tolerated; conflicting
    local edits will cause 'git pull' to fail, which is what we want
    -- we don't want to silently run makes against a dirty tree.
    The "Already up to date." case is suppressed to keep cron output
    quiet; any other pull output is surfaced to stderr."""
    if not os.path.isdir(os.path.join(kentTree, ".git")):
        print("ERROR: not a git working tree: %s" % kentTree,
              file=sys.stderr)
        return False
    result = subprocess.run(
        ["git", "-C", kentTree, "pull"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        print("ERROR: 'git pull' failed in %s:\n%s%s"
              % (kentTree, result.stdout, result.stderr),
              file=sys.stderr)
        return False
    out = result.stdout.strip()
    if out and out != "Already up to date.":
        print("# git pull in %s:\n%s" % (kentTree, out), file=sys.stderr)
    return True


def hgsql(query, db="hgcentral"):
    """Run hgsql -N -B and return rows as list of tuples (tab-split)."""
    out = subprocess.run(
        ["/cluster/bin/x86_64/hgsql", '-h', 'genome-centdb', "-N", "-B", "-e", query, db],
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
    """Build {clade: [assemblyId, ...]}.  dbs may mix GenArk accessions
    and UCSC native db names; dbDbClades may be empty when no native
    dbs are expected."""
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
    Only GenArk identifiers are used; UCSC native dbs are not in the
    AsmHub orderList files.

    If no matches are found in the expected clade directory, falls back
    to checking legacyAsmHub/legacy.orderList.tsv and works there instead.

    Returns cladeDir on success (so the caller can chain the make
    sequence), or None if there is nothing to do for this clade.
    """
    genarkIds = [a for a in asmIds if gcPattern.match(a)]
    if not genarkIds:
        return None

    # First try the expected clade directory
    cladeDir = os.path.join(
        kentTree, "src/hg/makeDb/doc/%sAsmHub" % clade)
    orderList = os.path.join(cladeDir, "%s.orderList.tsv" % clade)
    outPath = os.path.join(cladeDir, "tsv.otto")

    # orderList.tsv files occasionally contain Latin-1 bytes (e.g. in
    # Scandinavian fish names) that aren't valid UTF-8.  surrogateescape
    # round-trips those bytes through read+write byte-for-byte instead of
    # raising UnicodeDecodeError.
    matched = []
    foundIds = set()

    if os.path.isfile(orderList):
        with open(orderList, encoding="utf-8", errors="surrogateescape") as fh:
            for line in fh:
                for asmId in genarkIds:
                    if asmId in line:
                        matched.append(line)
                        foundIds.add(asmId)
                        break  # Don't match the same line multiple times

    # Look for IDs not found in main clade file
    notMatched = [asmId for asmId in genarkIds if asmId not in foundIds]
    if notMatched:
        legacyDir = os.path.join(
            kentTree, "src/hg/makeDb/doc/legacyAsmHub")
        legacyOrderList = os.path.join(legacyDir, "legacy.orderList.tsv")
        legacyOutPath = os.path.join(legacyDir, "tsv.otto")

        legacyMatched = []
        if os.path.isfile(legacyOrderList):
            with open(legacyOrderList, encoding="utf-8", errors="surrogateescape") as fh:
                for line in fh:
                    for asmId in notMatched:
                        if asmId in line:
                            legacyMatched.append(line)
                            foundIds.add(asmId)
                            break  # Don't match the same line multiple times

            if legacyMatched:
                # Write matches to legacy directory
                with open(legacyOutPath, "w", encoding="utf-8", errors="surrogateescape") as fh:
                    fh.writelines(legacyMatched)

                # If we have matches from both main and legacy, handle legacy completely here
                if matched:
                    if not runGenArkMake(legacyDir):
                        print(f"# WARNING: make commands failed in legacy directory", file=sys.stderr)
                    # Main directory will be handled by normal return path below
                    # This allows both directories to be processed independently
                else:
                    # Found matches only in legacy
                    return legacyDir

    # Check for any IDs that still weren't found anywhere
    stillNotFound = [asmId for asmId in genarkIds if asmId not in foundIds]
    if stillNotFound:
        if not os.path.isfile(orderList):
            print("WARNING: missing %s" % orderList, file=sys.stderr)
        legacyOrderList = os.path.join(kentTree, "src/hg/makeDb/doc/legacyAsmHub/legacy.orderList.tsv")
        if not os.path.isfile(legacyOrderList):
            print("WARNING: missing %s" % legacyOrderList, file=sys.stderr)
        print("WARNING: no matches for %s in %s or legacy.orderList.tsv" %
              (stillNotFound, clade), file=sys.stderr)

    # If we have matches from main clade, write them and return main directory
    if matched:
        with open(outPath, "w", encoding="utf-8", errors="surrogateescape") as fh:
            fh.writelines(matched)
        return cladeDir

    # No matches found anywhere
    return None


# Sequence of make commands run in the clade AsmHub directory after
# tsv.otto is written.  Stops on the first failure.
genArkMakeCommands = [
    "time (make symLinks orderList=tsv.otto) >> dbg 2>&1",
    "time (make mkGenomes orderList=tsv.otto) >> dbg 2>&1",
    "time (make symLinks orderList=tsv.otto) >> dbg 2>&1",
    "time (make verifyTestDownload orderList=tsv.otto) >> test.down.log 2>&1",
    "time (make sendDownload orderList=tsv.otto) >> send.down.log 2>&1",
    "time (make verifyDownload orderList=tsv.otto) >> verify.down.log 2>&1",
]


def runGenArkMake(cladeDir):
    """Run the genArkMakeCommands sequence in cladeDir.  Uses bash so
    'time (...)' (a builtin on a subshell) and '>>' / '2>&1' work as
    written.  Returns True on success, False if any step fails (the
    chain stops at the first failure)."""
    for cmd in genArkMakeCommands:
        result = subprocess.run(
            cmd, shell=True, executable="/bin/bash", cwd=cladeDir,
        )
        if result.returncode != 0:
            print("# ERROR: exit %d from: %s -- stopping chain"
                  % (result.returncode, cmd), file=sys.stderr)
            return False
    return True


def genArkBuildDir(asmId):
    """Return /hive/data/genomes/asmHubs/allBuild/<3>/<3>/<3>/<3>/<asmId>
    for a full GenArk asmId (GC[AF]_<digits.version>_<asmName>).
    The four 3-char path segments come from the accession part only, so
    callers that only have the bare accession still get a valid parent
    directory, but the final segment requires the asmName too."""
    return ("/hive/data/genomes/asmHubs/allBuild/"
            "%s/%s/%s/%s/%s"
            % (asmId[0:3], asmId[4:7], asmId[7:10], asmId[10:13], asmId))


def runDoTrackDb(asmId, logPath=None):
    """Run the doTrackDb.bash script that sits at the top of asmId's
    GenArk hub build dir, the same way ottoRequestWatch.sh does.
    asmId must be the full <acc>_<asmName>.  When logPath is given,
    stdout+stderr are appended to that file; otherwise they go to
    /dev/null.  Returns True on success, False on failure (including
    when the script can't be found or isn't executable)."""
    hubBuild = genArkBuildDir(asmId)
    doTdb = os.path.join(hubBuild, "doTrackDb.bash")
    if not os.access(doTdb, os.X_OK):
        print("# ERROR: cannot find executable %s" % doTdb, file=sys.stderr)
        return False
    logFh = open(logPath, "a") if logPath else open(os.devnull, "w")
    try:
        result = subprocess.run(
            [doTdb], stdout=logFh, stderr=subprocess.STDOUT,
        )
    finally:
        logFh.close()
    if result.returncode != 0:
        print("# ERROR: %s exited %d" % (doTdb, result.returncode),
              file=sys.stderr)
        return False
    return True
