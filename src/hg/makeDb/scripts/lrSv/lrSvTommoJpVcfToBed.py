#!/usr/bin/env python3
"""Convert ToMMo JSV1 SURVIVOR-merged SV VCF to BED9+ for bigBed.

Usage:
    lrSvTommoJpVcfToBed.py input.vcf.gz output.bed
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType

# Colors by SV type (R,G,B)
SV_COLORS = {
    "DEL": "200,0,0",      # red
    "INS": "0,0,200",      # blue
}

def parseInfo(infoStr):
    """Parse INFO field into a dict."""
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
            qual = fields[5]
            info = parseInfo(fields[7])

            svTypeRaw = info.get("SVTYPE", ".")
            svType = normalizeSvType(svTypeRaw)
            end = int(info.get("END", pos))
            svLenRaw = int(float(info.get("SVLEN", "0")))  # VCF SVLEN for INS
            af = info.get("AF", "0")
            ac = int(info.get("AC", "0"))
            an = int(info.get("AN", "0"))

            # Mendelian error fields
            errRatio = info.get("ERROR_FAMILY_RATIO", ".")
            errNum = info.get("ERROR_FAMILY_NUM", "0")
            famNum = info.get("FAMILY_NUM", "0")

            # Handle nan/missing AF
            try:
                afFloat = float(af)
            except ValueError:
                afFloat = 0.0

            # BED is 0-based half-open
            chromStart = pos - 1

            # For INS, END == POS so the item has zero width; expand by 1 bp
            chromEnd = end
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1

            # svLen = reference span
            svLen = chromEnd - chromStart
            # insLen = size of inserted sequence (from SVLEN for INS)
            if svType in ("INS", "MEI"):
                insLen = abs(svLenRaw)
            else:
                insLen = 0

            # Score: map QUAL to 0-1000
            try:
                score = min(int(round(float(qual) * 2)), 1000)
            except ValueError:
                score = 0

            strand = "."
            color = SV_COLORS.get(svType, "100,100,100")

            # Handle missing error ratio
            try:
                errRatioFloat = float(errRatio)
            except ValueError:
                errRatioFloat = 0.0

            try:
                errNumInt = int(errNum)
            except ValueError:
                errNumInt = 0

            try:
                famNumInt = int(famNum)
            except ValueError:
                famNumInt = 0

            featLen = insLen if svType in ("INS", "MEI") else svLen
            name = svName(svType, featLen, ac)

            row = [
                chrom,
                str(chromStart),
                str(chromEnd),
                name,
                str(score),
                strand,
                str(chromStart),   # thickStart
                str(chromEnd),     # thickEnd
                color,
                svType,
                str(svLen),
                str(insLen),
                str(ac),
                f"{afFloat:.6f}",
                str(an),
                f"{errRatioFloat:.3f}",
                str(errNumInt),
                str(famNumInt),
            ]
            fOut.write("\t".join(row) + "\n")

if __name__ == "__main__":
    main()
