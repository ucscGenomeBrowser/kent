#!/usr/bin/env python3
"""
ottoRequestPush2.py - lib-using rewrite of ottoRequestPush.py.

Same behavior as ottoRequestPush.py: groups fromDb/toDb identifiers
from pending push requests (status=5) by clade, drives the per-clade
genArkMakeCommands sequence, rsyncs the UCSC-native .over.chain.gz to
both hgdownload hosts, and advances each request's status (6 on full
success, 7 on rsync failure, stays at 5 on clade-side failure).

Differences from ottoRequestPush.py: all shared helpers live in
ottoLib.py.  Push-specific helpers (pendingRequests, mark*,
pushUcscChain) and main() stay here.

This is a parallel script for review/cutover - the live cron continues
to invoke ottoRequestPush.py.
"""

import os
import subprocess
import sys
import tempfile
import atexit

import ottoLib

scriptDir = os.path.dirname(os.path.abspath(__file__))
# share the live cron's lock so the two scripts cannot run concurrently
# in the same cladeAsmHub directories
lockPath = os.path.join(scriptDir, "ottoRequestPush.lock")

# UCSC native .over.chain.gz files get rsync'd to both hgdownload hosts.
pushUser = "qateam"
pushHosts = ["hgdownload1.soe.ucsc.edu", "hgdownload3.gi.ucsc.edu"]


def pendingRequests():
    """Status=5 liftOver requests as [(id, fromDb, toDb), ...]."""
    rows = ottoLib.hgsql(
        "SELECT id, fromDb, toDb FROM ottoRequest "
        "WHERE status = 5 AND requestType = 'liftOver';"
    )
    return [(int(r[0]), r[1], r[2]) for r in rows]


def markComplete(reqIds):
    """Set status=6 on the given ottoRequest ids."""
    if not reqIds:
        return
    idList = ",".join(str(i) for i in sorted(reqIds))
    ottoLib.hgsql(
        "UPDATE ottoRequest SET status = 6 WHERE id IN (%s);" % idList)


def markFailed(reqIds):
    """Set status=7 (problems) on the given ottoRequest ids."""
    if not reqIds:
        return
    idList = ",".join(str(i) for i in sorted(reqIds))
    ottoLib.hgsql(
        "UPDATE ottoRequest SET status = 7 WHERE id IN (%s);" % idList)
    print("# ottoRequestPush.py marked status=7 for failing list: %s" % idList, file=sys.stderr)


def pushUcscChain(targetDb, queryDb):
    """rsync the .over.chain.gz to both hgdownload hosts under
    /goldenPath/<targetDb>/liftOver/.  targetDb must be a UCSC native db
    (the goldenPath tree only exists for native dbs); queryDb may be
    GenArk or native.  Source filename uses dot separators (e.g.
    ce11.GCA_000180635.4.over.chain.gz), destination filename uses
    CamelCase (ce11ToGCA_000180635.4.over.chain.gz); rsync renames in
    flight by giving the destination as a file path, not a directory.
    -L dereferences the lastz.<queryDb> symlink to the dated build dir.
    Pre-creates the destination directory with a separate ssh + mkdir
    -p.  Returns True on success, False on any failure."""
    QueryDb = queryDb[:1].upper() + queryDb[1:]
    src = ("/hive/data/genomes/%s/bed/lastz.%s/axtChain/%s.%s.over.chain.gz"
           % (targetDb, queryDb, targetDb, queryDb))
    if not os.path.isfile(src):
        print("# ERROR: pushUcscChain: missing source %s"
              % src, file=sys.stderr)
        return False
    dstDir = "/data/apache/htdocs/goldenPath/%s/liftOver" % targetDb
    dstFile = "%s/%sTo%s.over.chain.gz" % (dstDir, targetDb, QueryDb)
    for host in pushHosts:
        target = "%s@%s" % (pushUser, host)
        result = subprocess.run(
            ["ssh", target, "mkdir", "-p", dstDir],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            print("# ERROR: pushUcscChain: mkdir failed on %s: %s"
                  % (host, result.stderr.strip()), file=sys.stderr)
            return False
        result = subprocess.run(
            ["rsync", "-avL", src, "%s:%s" % (target, dstFile)],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            print("# ERROR: pushUcscChain: rsync to %s failed: %s"
                  % (host, result.stderr.strip()), file=sys.stderr)
            return False
    return True


def main():
    # Set up temporary log file for capturing all output
    pid = os.getpid()
    logFile = f"/dev/shm/ottoPush.{pid}.txt"

    # Save original stdout/stderr for potential error reporting
    originalStderr = sys.stderr

    # Ensure cleanup happens even if script is killed
    def cleanup():
        if os.path.exists(logFile):
            os.remove(logFile)

    atexit.register(cleanup)

    try:
        # Redirect stdout and stderr to the log file
        with open(logFile, 'w') as log:
            sys.stdout = log
            sys.stderr = log

            lockFh = ottoLib.acquireSingletonLock(lockPath)  # noqa: F841
            requests = pendingRequests()
            if not requests:
                return
            dbs = set()
            for _, fromDb, toDb in requests:
                dbs.update((fromDb, toDb))
            accessions = {db for db in dbs if ottoLib.gcPattern.match(db)}
            dbDbClades = ottoLib.loadDbDbClades()
            genarkInfo = ottoLib.lookupGenark(accessions)
            grouped = ottoLib.groupByClade(dbs, dbDbClades, genarkInfo)

            # bring the otto kent tree up to date before any cladeAsmHub make
            if not ottoLib.gitPullKentTree():
                sys.exit(1)

            def cladeOf(db):
                if ottoLib.gcPattern.match(db):
                    info = genarkInfo.get(db)
                    return info[1] if info else None
                return dbDbClades.get(db)

            succeededClades = set()
            failedClades = set()
            for clade in sorted(grouped):
                cladeDir = ottoLib.writeCladeTsv(clade, grouped[clade])
                if cladeDir is None:
                    continue
                if ottoLib.runGenArkMake(cladeDir):
                    succeededClades.add(clade)
                else:
                    failedClades.add(clade)

            # GenArk-target directions are pushed by the per-clade make chain
            # above; UCSC-native-target directions are pushed by rsync to the
            # hgdownload hosts.  A request advances to status=6 only when ALL
            # of its directions succeed.  Clade-side failures leave the request
            # at status=5 (existing behavior).  rsync failures move it to
            # status=7 to alert examination.
            completedIds = []
            failedIds = []
            cladeFailures = []
            pushFailures = []
            for reqId, fromDb, toDb in requests:
                genarkFailedClades = set()
                genarkIncomplete = False
                for db in (fromDb, toDb):
                    if not ottoLib.gcPattern.match(db):
                        continue
                    clade = cladeOf(db)
                    if clade is None:
                        genarkIncomplete = True       # missing clade info; retry
                    elif clade in failedClades:
                        genarkFailedClades.add(clade)
                    elif clade not in succeededClades:
                        genarkIncomplete = True       # not yet pushed; retry
                if genarkFailedClades:
                    cladeFailures.append(
                        (reqId, fromDb, toDb, sorted(genarkFailedClades)))
                    continue
                if genarkIncomplete:
                    continue

                pushOk = True
                pushFailedDirs = []
                for target, query in [(fromDb, toDb), (toDb, fromDb)]:
                    if ottoLib.gcPattern.match(target):
                        continue                      # GenArk side already handled
                    if not pushUcscChain(target, query):
                        pushFailedDirs.append("%s -> %s" % (target, query))
                        pushOk = False
                        break
                if pushOk:
                    completedIds.append(reqId)
                else:
                    failedIds.append(reqId)
                    pushFailures.append((reqId, fromDb, toDb, pushFailedDirs))

            markComplete(completedIds)
            markFailed(failedIds)

            if cladeFailures:
                print("# the following request(s) stay at status=5 due to failed "
                      "clade pushes:")
                for reqId, fromDb, toDb, badClades in cladeFailures:
                    print("#   id=%d %s -> %s (failed clade(s): %s)"
                          % (reqId, fromDb, toDb, ", ".join(badClades)))

            if pushFailures:
                print("# the following request(s) set to status=7 due to rsync "
                      "failures:")
                for reqId, fromDb, toDb, dirs in pushFailures:
                    print("#   id=%d %s -> %s (failed: %s)"
                          % (reqId, fromDb, toDb, "; ".join(dirs)))

            # Restore stdout/stderr before potential exit
            sys.stdout = sys.__stdout__
            sys.stderr = sys.__stderr__

    except Exception as e:
        # Restore stdout/stderr first
        sys.stdout = sys.__stdout__
        sys.stderr = originalStderr

        # Print the captured log to stderr for cron visibility
        if os.path.exists(logFile):
            with open(logFile, 'r') as log:
                for line in log:
                    print(line, end='', file=originalStderr)

        # Print the exception that caused the failure
        print(f"# FATAL ERROR: {e}", file=originalStderr)
        sys.exit(1)

    except SystemExit as e:
        # Handle sys.exit() calls - restore streams first
        sys.stdout = sys.__stdout__
        sys.stderr = originalStderr

        # If exit code is non-zero, print the log for debugging
        if e.code != 0:
            if os.path.exists(logFile):
                with open(logFile, 'r') as log:
                    for line in log:
                        print(line, end='', file=originalStderr)

        # Re-raise the SystemExit
        raise

    finally:
        # Always restore streams and cleanup
        sys.stdout = sys.__stdout__
        sys.stderr = sys.__stderr__
        cleanup()


if __name__ == "__main__":
    main()
