#!/usr/bin/env python3
"""Convert the deCODE high-confidence long-read SV VCF to BED9+ for bigBed.

Usage:
    lrSvDecodeVcfToBed.py input.vcf.gz output.bed
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType

SV_COLORS = {
    "DEL": "200,0,0",       # red
    "INS": "0,0,200",       # blue
    "INSDEL": "140,0,200",  # purple (combined INS/DEL)
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
            trrBegin = info.get("TRRBEGIN", ".")
            trrEnd = info.get("TRREND", ".")

            if trrBegin == ".":
                trrBegin = ""
            if trrEnd == ".":
                trrEnd = ""

            chromStart = pos - 1
            chromEnd = end
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1

            svLen = chromEnd - chromStart
            if svType in ("INS", "MEI"):
                insLen = abs(svLenRaw)
            else:
                insLen = 0  # DEL/INSDEL

            # placeholder; awaiting real values from deCODE (#35059 ...)
            ac = 50

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
                trrBegin,
                trrEnd,
            ]
            fOut.write("\t".join(row) + "\n")


if __name__ == "__main__":
    main()
