#!/usr/bin/env python3
"""
featureBitsSnapshot.py - maintain an append-only snapshot of
featureBits coverage percentages used by ottoRequestView.cgi.

For every (fromDb, toDb) pair in hgcentraltest.ottoRequest, reads the
fb.<src>.chain<Qry>Link.txt produced by the lastz/chain/net pipeline
and records its "(X.Y%)" value in
  /data/apache/trash/ottoRequestFeatureBitsPct.json
keyed as "<src>\\t<qry>".  Once a pair is recorded the value never
changes, so subsequent runs only inspect pairs that aren't already in
the snapshot.

Intended cadence: invoked from ottoRequestWatch.sh on every cron tick.
Per-tick cost in steady state is zero (no new pairs to measure).
"""

import fcntl
import json
import os
import re
import subprocess
import sys
import tempfile
import time

scriptDir     = os.path.dirname(os.path.abspath(__file__))
lockPath      = os.path.join(scriptDir, "featureBitsSnapshot.lock")
SNAPSHOT_PATH = "/data/apache/trash/ottoRequestFeatureBitsPct.json"

HIVE_GENOMES  = "/hive/data/genomes"
ASMHUB_ROOT   = HIVE_GENOMES + "/asmHubs"

gcPattern = re.compile(r"^GC[AF]_")
pctRegex  = re.compile(r"\(([\d.]+)%\)")


def acquireSingletonLock():
    """Exclusive flock on lockPath for the lifetime of the process.
    Silent exit 0 if another instance holds the lock so cron doesn't
    email on every overlapping tick."""
    fh = open(lockPath, "a+")
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        sys.exit(0)
    fh.seek(0)
    fh.truncate()
    fh.write("%d\n" % os.getpid())
    fh.flush()
    return fh


def hgsql(sql, db="hgcentraltest"):
    """Run hgsql -N -B and return rows as a list of tab-split tuples."""
    out = subprocess.run(
        ["/cluster/bin/x86_64/hgsql", "-N", "-B", "-e", sql, db],
        check=True, capture_output=True, text=True,
    ).stdout
    return [tuple(line.split("\t")) for line in out.splitlines() if line]


def loadSnapshot():
    """Return the existing pct dict from the snapshot file, or {} if
    the file is missing or malformed."""
    try:
        with open(SNAPSHOT_PATH) as f:
            data = json.load(f)
    except (OSError, ValueError):
        return {}
    return data.get("pct", {}) or {}


def writeSnapshot(pct):
    """Write pct to SNAPSHOT_PATH atomically.  tempfile in the same
    directory + os.replace() guarantees readers never see a partial
    write."""
    dstDir = os.path.dirname(SNAPSHOT_PATH)
    fd, tmpPath = tempfile.mkstemp(
        dir=dstDir,
        prefix=os.path.basename(SNAPSHOT_PATH) + ".",
    )
    try:
        with os.fdopen(fd, "w") as tmp:
            json.dump({
                "ts":  time.strftime("%Y-%m-%d %H:%M:%S"),
                "pct": pct,
            }, tmp, indent=2, sort_keys=True)
        os.chmod(tmpPath, 0o664)
        os.replace(tmpPath, SNAPSHOT_PATH)
    except BaseException:
        try:
            os.unlink(tmpPath)
        except OSError:
            pass
        raise


def pendingPairs():
    """Return a set of (src, qry) tuples covering both directions for
    every row in ottoRequest."""
    rows = hgsql(
        "SELECT fromDb, toDb FROM ottoRequest "
        "WHERE fromDb != '' AND toDb != '';"
    )
    pairs = set()
    for fromDb, toDb in rows:
        pairs.add((fromDb, toDb))
        pairs.add((toDb, fromDb))
    return pairs


def lookupGenarkNames(accessions):
    """Bulk-lookup {gcAccession: asmName} for GenArk accessions in one
    hgsql call.  Mirrors ottoRequestPush.lookupGenark() and the CGI's
    loadGenarkNames()."""
    if not accessions:
        return {}
    quoted = ",".join("'%s'" % a for a in sorted(accessions))
    rows = hgsql(
        "SELECT gcAccession, asmName FROM genark "
        "WHERE gcAccession IN (%s);" % quoted
    )
    return {acc: asmName for acc, asmName in rows}


def hubBuildDir(acc, genarkAsmName):
    """Same path resolution as ottoRequestView.cgi's hubBuildDir():
       GenArk -> asmHubs/{genbank,refseq}Build/<XXX>/<XXX>/<XXX>/<acc>_<asmName>
       UCSC native -> /hive/data/genomes/<db>
    Returns None when asmName is unknown or native dir is missing."""
    if not acc:
        return None
    if gcPattern.match(acc) and len(acc) >= 13:
        asmName = genarkAsmName.get(acc)
        if not asmName:
            return None
        src    = acc[:3]
        sub    = "refseqBuild" if src == "GCF" else "genbankBuild"
        digits = acc[4:].split(".", 1)[0]
        if len(digits) < 9:
            return None
        return "%s/%s/%s/%s/%s/%s/%s_%s" % (
            ASMHUB_ROOT, sub, src,
            digits[0:3], digits[3:6], digits[6:9],
            acc, asmName,
        )
    candidate = "%s/%s" % (HIVE_GENOMES, acc)
    if os.path.isdir(candidate):
        return candidate
    return None


def measurePct(src, qry, genarkAsmName):
    """Read fb.<src>.chain<Qry>Link.txt and return its 'X.Y%' string,
    or None if the file isn't present yet (alignment still in
    progress) or contains no parenthesized percentage."""
    bdir = hubBuildDir(src, genarkAsmName)
    if not bdir:
        return None
    sub = "trackData" if "/asmHubs/" in bdir else "bed"
    Qry = qry[:1].upper() + qry[1:]
    path = "%s/%s/lastz.%s/fb.%s.chain%sLink.txt" % (
        bdir, sub, qry, src, Qry,
    )
    try:
        with open(path) as f:
            txt = f.read()
    except OSError:
        return None
    m = pctRegex.search(txt)
    return (m.group(1) + "%") if m else None


def main():
    lockFh = acquireSingletonLock()  # noqa: F841 - keep ref alive
    snapshot = loadSnapshot()
    pairs = pendingPairs()
    # only inspect pairs not yet recorded; the values are immutable
    # once an alignment completes, so this stays append-only
    todo = [p for p in pairs if "%s\t%s" % p not in snapshot]
    if not todo:
        return
    gcAccs = {db for p in todo for db in p if gcPattern.match(db)}
    genarkAsmName = lookupGenarkNames(gcAccs)
    added = 0
    for src, qry in todo:
        pct = measurePct(src, qry, genarkAsmName)
        if pct is not None:
            snapshot["%s\t%s" % (src, qry)] = pct
            added += 1
    if added:
        writeSnapshot(snapshot)


if __name__ == "__main__":
    main()
