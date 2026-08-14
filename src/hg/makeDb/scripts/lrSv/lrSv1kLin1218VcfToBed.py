#!/usr/bin/env python3
"""Convert 1000 Genomes linear long-read SV VCF (1,218 samples) to BED9+ for bigBed.

Usage:
    lrSv1kLin1218VcfToBed.py input.vcf.gz output.bed

Input VCF was merged with Truvari v5.2.0 and annotated with bcftools fill-tags
for population-level AC/AN/AF per 1000 Genomes super-populations (EUR, AMR,
EAS, AFR, SAS). Only DEL and INS variant types are present.
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType

SV_COLORS = {
    "DEL": "200,0,0",
    "INS": "0,0,200",
}


def parseInfo(infoStr):
    d = {}
    for item in infoStr.split(";"):
        if "=" in item:
            k, v = item.split("=", 1)
            d[k] = v
        else:
            d[item] = True
    return d


def toInt(s, default=0):
    if not s or s in (".", "NA"):
        return default
    try:
        return int(float(s))
    except ValueError:
        return default


def toFloat(s, default=0.0):
    if not s or s in (".", "NA"):
        return default
    try:
        return float(s)
    except ValueError:
        return default


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile, outFile = sys.argv[1], sys.argv[2]
    opener = gzip.open if inFile.endswith(".gz") else open

    nWritten = 0
    with opener(inFile, "rt") as fIn, open(outFile, "w") as fOut:
        for line in fIn:
            if line.startswith("#"):
                continue

            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])
            info = parseInfo(fields[7])

            svType = normalizeSvType(info.get("SVTYPE", "."))
            svLenRaw = toInt(info.get("SVLEN", "0"))
            end = toInt(info.get("END", str(pos)))

            chromStart = pos - 1
            chromEnd = end
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1

            svLen = chromEnd - chromStart
            insLen = abs(svLenRaw) if svType in ("INS", "MEI") else 0

            ac = toInt(info.get("AC", "0"))
            an = toInt(info.get("AN", "0"))
            af = toFloat(info.get("AF", "0"))
            afAfr = toFloat(info.get("AF_AFR", "0"))
            afAmr = toFloat(info.get("AF_AMR", "0"))
            afEas = toFloat(info.get("AF_EAS", "0"))
            afEur = toFloat(info.get("AF_EUR", "0"))
            afSas = toFloat(info.get("AF_SAS", "0"))
            ns = toInt(info.get("NS", "0"))
            numConsolidated = toInt(info.get("NumConsolidated", "0"))

            color = SV_COLORS.get(svType, "100,100,100")
            featLen = insLen if svType in ("INS", "MEI") else svLen
            name = svName(svType, featLen, ac)

            row = [
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
                str(an),
                f"{af:.6f}",
                f"{afAfr:.6f}",
                f"{afAmr:.6f}",
                f"{afEas:.6f}",
                f"{afEur:.6f}",
                f"{afSas:.6f}",
                str(ns),
                str(numConsolidated),
            ]
            fOut.write("\t".join(row) + "\n")
            nWritten += 1

    print(f"Wrote {nWritten} records to {outFile}", file=sys.stderr)


if __name__ == "__main__":
    main()
