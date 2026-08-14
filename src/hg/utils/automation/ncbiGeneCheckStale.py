#!/usr/bin/env python3
"""
ncbiGeneCheckStale.py - bulk-check GenArk asmHub ncbiGene track builds
against their NCBI source GFF3, to find which ones need doNcbiGene.bash
run (or re-run).

Everything is derived from the assemblyId alone (e.g.
GCA_046562895.1_mTriInu1_haplotype_2):
  - prefix (GCA/GCF) and the 9-digit accession split into 3-digit chunks
    give the NCBI mirror path:
      /hive/data/outside/ncbi/genomes/{GCA,GCF}/ddd/ddd/ddd/{asmId}/
          {asmId}_genomic.gff.gz
  - the same ddd/ddd/ddd split gives the asmHub build path:
      /hive/data/genomes/asmHubs/{genbankBuild,refseqBuild}/{GCA,GCF}/
          ddd/ddd/ddd/{asmId}/trackData/ncbiGene/

Staleness test mirrors doNcbiGene.bash's own check
(gffFile -nt $asmId.ncbiGene.bb): if the source gff.gz is newer than the
track's .bb, the step needs to be (re)run.

usage:
   ncbiGeneCheckStale.py asmId [asmId ...]
   ncbiGeneCheckStale.py listFile.txt
   ncbiGeneCheckStale.py listFile.txt asmId ...   # any mix of both

listFile.txt: one assemblyId per line, taken from the first
whitespace-separated column; blank lines and lines starting with '#'
are skipped.  (This is the same shape as the asmHub assembly list files
already used elsewhere in the build scripts.)
"""

import sys
import os
import re
import time

NCBI_ROOT = "/hive/data/outside/ncbi/genomes"
HUB_ROOT = "/hive/data/genomes/asmHubs"

BUILD_DIR_NAME = {"GCA": "genbankBuild", "GCF": "refseqBuild"}

ASM_ID_RE = re.compile(r"^(GCA|GCF)_(\d{3})(\d{3})(\d{3})\.\d+_")


def splitAsmId(asmId):
    """returns (prefix, d1, d2, d3) or None if asmId doesn't match the
    expected GCA_/GCF_ + 9 digit accession pattern"""
    m = ASM_ID_RE.match(asmId)
    if not m:
        return None
    return m.groups()


def pathsFor(asmId):
    """all the paths that matter for this asmId, or None if the id
    itself doesn't parse"""
    parts = splitAsmId(asmId)
    if parts is None:
        return None
    prefix, d1, d2, d3 = parts
    ncbiDir = f"{NCBI_ROOT}/{prefix}/{d1}/{d2}/{d3}/{asmId}"
    buildDir = f"{HUB_ROOT}/{BUILD_DIR_NAME[prefix]}/{prefix}/{d1}/{d2}/{d3}/{asmId}"
    trackDir = f"{buildDir}/trackData/ncbiGene"
    return {
        "gffFile": f"{ncbiDir}/{asmId}_genomic.gff.gz",
        "buildDir": buildDir,
        "trackDir": trackDir,
        "bbFile": f"{trackDir}/{asmId}.ncbiGene.bb",
    }


def checkOne(asmId):
    """returns (status, detail-dict) for one assemblyId"""
    p = pathsFor(asmId)
    if p is None:
        return "BAD_ASM_ID", {}

    if not os.path.exists(p["gffFile"]):
        return "NO_NCBI_GFF", p

    if not os.path.isdir(p["trackDir"]):
        return "NO_BUILD_DIR", p

    if not os.path.exists(p["bbFile"]):
        return "NEVER_RUN", p

    gffTime = os.path.getmtime(p["gffFile"])
    bbTime = os.path.getmtime(p["bbFile"])
    if gffTime > bbTime:
        p["gffTime"] = gffTime
        p["bbTime"] = bbTime
        return "STALE", p

    return "UP_TO_DATE", p


def readIdsFromFile(path):
    ids = []
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            ids.append(line.split()[0])
    return ids


def collectIds(args):
    ids = []
    for arg in args:
        if os.path.isfile(arg):
            ids.extend(readIdsFromFile(arg))
        else:
            ids.append(arg)
    return ids


def fmtAge(seconds):
    days = seconds / 86400.0
    return f"{days:.1f}d"


def main():
    if len(sys.argv) < 2:
        sys.stderr.write(__doc__)
        sys.exit(255)

    ids = collectIds(sys.argv[1:])

    counts = {}
    for asmId in ids:
        status, detail = checkOne(asmId)
        counts[status] = counts.get(status, 0) + 1
        extra = ""
        if status == "STALE":
            age = fmtAge(detail["gffTime"] - detail["bbTime"])
            extra = f"  (gff is {age} newer than .bb)"
        print(f"{status}\t{asmId}{extra}")

    sys.stderr.write("\n# summary:\n")
    for status in ("UP_TO_DATE", "STALE", "NEVER_RUN", "NO_BUILD_DIR",
                    "NO_NCBI_GFF", "BAD_ASM_ID"):
        if status in counts:
            sys.stderr.write(f"#   {status}: {counts[status]}\n")


if __name__ == "__main__":
    main()
