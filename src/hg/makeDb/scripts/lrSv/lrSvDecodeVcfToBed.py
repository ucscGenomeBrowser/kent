#!/usr/bin/env python3
"""Convert the deCODE high-confidence long-read SV VCF to BED9+ for bigBed.

Usage:
    lrSvDecodeVcfToBed.py input.vcf.gz output.bed

The source VCF has no allele count, so the AC column is left empty (it used to
carry a fake placeholder of 50). The source also lists some variants more than
once (byte-identical records); those duplicate rows are collapsed here.
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType, svColor


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

    seen = set()
    nIn = 0
    nDup = 0
    with opener(inFile, "rt") as fIn, open(outFile, "w") as fOut:
        for line in fIn:
            if line.startswith("#"):
                continue
            nIn += 1

            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])
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

            color = svColor(svType)

            featLen = insLen if svType in ("INS", "MEI") else svLen
            # deCODE publishes no allele count, so omit AC from the name.
            name = svName(svType, featLen)

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
                "",          # AC: deCODE has none (was a fake placeholder of 50)
                trrBegin,
                trrEnd,
            ]
            line_out = "\t".join(row)
            if line_out in seen:
                nDup += 1
                continue
            seen.add(line_out)
            fOut.write(line_out + "\n")

    print(f"deCODE: {nIn:,} input records, {nDup:,} duplicate rows dropped, "
          f"{nIn - nDup:,} written", file=sys.stderr)


if __name__ == "__main__":
    main()
