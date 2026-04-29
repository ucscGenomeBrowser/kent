#!/usr/bin/env python3
"""
ottoRequestPush.py - group fromDb/toDb identifiers from pending push
requests (status=5) by clade.

Output: dict[clade] -> sorted list of assembly identifiers, where each
identifier is "<gcAccession>_<asmName>" for GenArk accessions, or the
plain UCSC db name for native dbs.
"""

import os
import re
import subprocess
import sys
from collections import defaultdict

scriptDir = os.path.dirname(os.path.abspath(__file__))
cladeTsv = os.path.join(scriptDir, "dbDb.name.clade.tsv")
gcPattern = re.compile(r"^GC[AF]_")

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


def pendingDbs():
    """Distinct fromDb/toDb values across status=5 liftOver requests."""
    rows = hgsql(
        "SELECT fromDb, toDb FROM ottoRequest "
        "WHERE status = 5 AND requestType = 'liftOver';"
    )
    dbs = set()
    for fromDb, toDb in rows:
        dbs.add(fromDb)
        dbs.add(toDb)
    return dbs


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


def main():
    dbs = pendingDbs()
    if not dbs:
        return
    accessions = {db for db in dbs if gcPattern.match(db)}
    grouped = groupByClade(dbs, loadDbDbClades(), lookupGenark(accessions))
    for clade in sorted(grouped):
        print("%s:" % clade)
        for asmId in grouped[clade]:
            print("  %s" % asmId)


if __name__ == "__main__":
    main()
