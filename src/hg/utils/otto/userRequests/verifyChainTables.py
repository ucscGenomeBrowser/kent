#!/usr/bin/env python3
"""
verifyChainTables.py - verify hgcentraltest.liftOverChain and
hgcentraltest.quickLiftChain are in sync.

The otto pipeline populates both tables together (Phase 3 of
ottoRequestWatch.sh installs liftOverChain + quickLiftChain rows
side-by-side via installLinks()).  Any (fromDb, toDb) pair present in
one but not the other indicates a partial backfill, a half-completed
alignment, or a manual edit that needs human attention.

Reports the row counts in each table and lists any disagreement.

Exit status:
  0 - tables agree
  1 - at least one row of disagreement
"""

import subprocess
import sys


def hgsql(query, db="hgcentraltest"):
    out = subprocess.run(
        ["/cluster/bin/x86_64/hgsql", "-N", "-B", "-e", query, db],
        check=True, capture_output=True, text=True,
    ).stdout
    return [tuple(line.split("\t")) for line in out.splitlines() if line]


def loadPairs(table):
    return set(hgsql("SELECT fromDb, toDb FROM %s;" % table))


def printDiff(label, pairs):
    print("\nIn %s (%d):" % (label, len(pairs)))
    for fromDb, toDb in pairs:
        print("  %s\t%s" % (fromDb, toDb))


def main():
    loPairs = loadPairs("liftOverChain")
    qlPairs = loadPairs("quickLiftChain")
    print("liftOverChain  has %d rows" % len(loPairs))
    print("quickLiftChain has %d rows" % len(qlPairs))
    inLoOnly = sorted(loPairs - qlPairs)
    inQlOnly = sorted(qlPairs - loPairs)
    if not inLoOnly and not inQlOnly:
        print("tables agree.")
        return 0
    if inLoOnly:
        printDiff("liftOverChain but not in quickLiftChain", inLoOnly)
    if inQlOnly:
        printDiff("quickLiftChain but not in liftOverChain", inQlOnly)
    return 1


if __name__ == "__main__":
    sys.exit(main())
