#!/usr/bin/env python3
"""Convert a GA4K Jasmine-merged SV VCF (site-only) to BED9+ for bigBed.

Usage:
    lrSvGa4kSvVcfToBed.py input.vcf.gz output.bed
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType

SV_COLORS = {
    "DEL": "200,0,0",      # red
    "INS": "0,0,200",      # blue
    "DUP": "0,160,0",      # green
    "INV": "230,140,0",    # orange
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


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile, outFile = sys.argv[1], sys.argv[2]
    opener = gzip.open if inFile.endswith(".gz") else open

    with opener(inFile, "rt") as fIn, open(outFile, "w") as fOut:
        for line in fIn:
            if line.startswith("#"):
                continue

            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])
            rowName = fields[2]
            info = parseInfo(fields[7])

            svTypeRaw = info.get("SVTYPE", ".")
            svType = normalizeSvType(svTypeRaw)
            end = int(info.get("END", pos))
            svLenRaw = int(float(info.get("SVLEN", "0")))
            af = float(info.get("SVF", "0"))
            svc = int(info.get("SVC", "0"))
            svn = int(info.get("SVN", "0"))

            chromStart = pos - 1
            chromEnd = end
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1

            svLen = chromEnd - chromStart
            if svType in ("INS", "MEI"):
                insLen = abs(svLenRaw)
            else:
                insLen = 0

            # AC: GA4K site-level VCF has no AC. Approximate as 2 * svn * af
            # (diploid alleles * variant frequency).
            ac = int(round(2 * svn * af))

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
                f"{af:.6f}",
                str(svc),
                str(svn),
            ]
            fOut.write("\t".join(row) + "\n")


if __name__ == "__main__":
    main()
