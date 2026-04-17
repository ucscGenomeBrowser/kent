#!/usr/bin/env python3
"""Convert a SURVIVOR-merged SV VCF (site-only) to BED9+ for bigBed.

Usage:
    lrSvVcfToBed.py input.vcf.gz output.bed
"""

import gzip
import sys

# Colors by SV type (R,G,B)
SV_COLORS = {
    "DEL": "200,0,0",      # red
    "INS": "0,0,200",      # blue
    "DUP": "0,160,0",      # green
    "INV": "230,140,0",    # orange
    "TRA": "140,0,200",    # purple
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

def suppVecToList(suppVec):
    """Convert binary support vector to comma-separated 1-based sample indices."""
    indices = []
    for i, c in enumerate(suppVec):
        if c == "1":
            indices.append(str(i + 1))
    return ",".join(indices) if indices else ""

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
            name = fields[2]
            qual = fields[5]
            info = parseInfo(fields[7])

            svType = info.get("SVTYPE", ".")
            end = int(info.get("END", pos))
            svLen = int(float(info.get("SVLEN", "0")))
            af = float(info.get("AF", "0"))
            supp = int(info.get("SUPP", "0"))
            ciPos = info.get("CIPOS", "0,0")
            ciEnd = info.get("CIEND", "0,0")
            chr2 = info.get("CHR2", ".")
            strands = info.get("STRANDS", "+-")
            suppVec = info.get("SUPP_VEC", "")

            # BED is 0-based half-open
            chromStart = pos - 1

            # For INS, END == POS so the item has zero width; expand by 1 bp
            chromEnd = end
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1

            # Score: map QUAL to 0-1000
            try:
                score = min(int(round(float(qual) * 2)), 1000)
            except ValueError:
                score = 0

            # Strand from first character of STRANDS field
            strand = strands[0] if strands and strands[0] in "+-" else "."

            # Absolute SV length
            absSvLen = abs(svLen)

            color = SV_COLORS.get(svType, "100,100,100")

            # sampleList from SUPP_VEC
            sampleList = suppVecToList(suppVec)

            # end2 for TRA; empty for non-TRA so skipEmptyFields hides them
            end2 = str(end) if svType == "TRA" else ""
            chr2Out = chr2 if svType == "TRA" else ""

            # For TRA, chromEnd is the position on chr1 side, not chr2
            if svType == "TRA":
                chromEnd = chromStart + 1

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
                str(absSvLen),
                f"{af:.6f}",
                str(supp),
                ciPos,
                ciEnd,
                chr2Out,
                end2,
                sampleList,
            ]
            fOut.write("\t".join(row) + "\n")

if __name__ == "__main__":
    main()
