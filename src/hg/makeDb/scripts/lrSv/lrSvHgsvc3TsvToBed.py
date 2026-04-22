#!/usr/bin/env python3
"""Convert HGSVC3 final annotation TSVs (insdel + inv) to a merged BED9+ file.

Usage:
    lrSvHgsvc3TsvToBed.py insdel.tsv.gz inv.tsv.gz output.bed
"""

import csv
import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType

SV_COLORS = {
    "DEL": "200,0,0",      # red
    "INS": "0,0,200",      # blue
    "INV": "230,140,0",    # orange
}


def openTsv(path):
    return gzip.open(path, "rt") if path.endswith(".gz") else open(path, "rt")


def countSamples(mergeSamples):
    """Return (haplotypeCount, uniqSampleCount).

    MERGE_SAMPLES is a comma-separated list of haplotype IDs like
    HG00096-h1,HG00096-h2,HG00514-un. Haplotype count is the length of that
    list. Unique sample count strips the -h1/-h2/-un suffix.
    """
    if not mergeSamples:
        return 0, 0
    parts = mergeSamples.split(",")
    uniq = set()
    for p in parts:
        p = p.strip()
        if not p:
            continue
        for suf in ("-h1", "-h2", "-un"):
            if p.endswith(suf):
                p = p[: -len(suf)]
                break
        uniq.add(p)
    return len(parts), len(uniq)


def emit(outF, row, typeExtra):
    """Write one BED row. typeExtra is dict of the fields that differ
    between insdel and inv.
    """
    chrom = row["#CHROM"]
    pos = int(row["POS"])  # 0-based in source TSV based on sample inspection
    end = int(row["END"])
    svType = normalizeSvType(row["SVTYPE"])
    try:
        svLenSrc = int(row["SVLEN"])
    except ValueError:
        svLenSrc = 0
    absSvLen = abs(svLenSrc)

    # Source coords are already 0-based half-open (chr1 POS=10669 END=10901
    # matches a 232-bp deletion starting at 10670 in 1-based). Use as-is.
    chromStart = pos
    chromEnd = end
    if chromEnd <= chromStart:
        chromEnd = chromStart + 1

    svLen = chromEnd - chromStart
    if svType in ("INS", "MEI"):
        insLen = absSvLen
    else:
        insLen = 0

    haploCount, sampleCount = countSamples(row.get("MERGE_SAMPLES", ""))
    ac = haploCount

    color = SV_COLORS.get(svType, "100,100,100")

    refTrf = row.get("REF_TRF", "")
    try:
        refSd = float(row.get("REF_SD", "0") or 0)
    except ValueError:
        refSd = 0.0

    featLen = insLen if svType in ("INS", "MEI") else svLen
    name = svName(svType, featLen, ac)

    bedRow = [
        chrom,
        str(chromStart),
        str(chromEnd),
        name,
        "0",
        ".",
        str(chromStart),
        str(chromEnd),
        color,
        svType,
        str(svLen),
        str(insLen),
        str(ac),
        str(sampleCount),
        typeExtra.get("homRef", ""),
        typeExtra.get("homTig", ""),
        typeExtra.get("regionRefInner", ""),
        typeExtra.get("te", ""),
        refTrf,
        f"{refSd:.6f}",
        row.get("MERGE_SAMPLES", ""),
        row.get("WIN_500", ""),
        row.get("WIN_2K", ""),
    ]
    outF.write("\t".join(bedRow) + "\n")


def main():
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    insdelPath, invPath, outPath = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(outPath, "w") as outF:
        # insertions + deletions
        with openTsv(insdelPath) as fIn:
            reader = csv.DictReader(fIn, delimiter="\t")
            for row in reader:
                emit(outF, row, {
                    "homRef": row.get("HOM_REF", ""),
                    "homTig": row.get("HOM_TIG", ""),
                    "te": row.get("TE", ""),
                })

        # inversions
        with openTsv(invPath) as fIn:
            reader = csv.DictReader(fIn, delimiter="\t")
            for row in reader:
                emit(outF, row, {
                    "regionRefInner": row.get("RGN_REF_INNER", ""),
                })


if __name__ == "__main__":
    main()
