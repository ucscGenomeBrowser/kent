#!/usr/bin/env python3
"""Convert a ClinPred all-missense score table into four per-ALT wig files.

Input is the tab-separated table distributed by the ClinPred authors:
    Chr  Start  Ref  Alt  ClinPred_Score
with 1-based positions, no 'chr' prefix, and one row per scored missense
SNV. Three rows are present at every scored exome position (one per
non-reference base).

Output is a.wig, c.wig, g.wig, t.wig in the current directory.

Convention: at every position covered by at least one ClinPred score,
the reference base and any synonymous ALTs (those with no row in the
input) are emitted as 0.0 on their respective tracks; positions with no
exome coverage are emitted as gaps. This lets the user distinguish
"reference / no missense effect" from "no data here".
"""

import sys
import numpy as np

NUC_IDX = {"A": 0, "C": 1, "G": 2, "T": 3}
OUT_NAMES = ["a.wig", "c.wig", "g.wig", "t.wig"]


def parseChromSizes(fname):
    sizes = {}
    for line in open(fname):
        chrom, size = line.strip().split()
        sizes[chrom] = int(size)
    return sizes


def initArrays(chromLen):
    return [np.full(chromLen, -1, dtype=float) for _ in range(4)]


def writeChrom(chrom, arrays, outFhs):
    arrSum = np.sum(arrays, axis=0)
    chromLen = arrays[0].size
    for nucIdx in range(4):
        arr = arrays[nucIdx]
        ofh = outFhs[nucIdx]
        stretch = []
        stretchStart = 0
        for i in range(chromLen):
            val = arr[i]
            hasData = (arrSum[i] != -4)
            if hasData:
                if not stretch:
                    stretchStart = i
                stretch.append(0.0 if val == -1 else val)
            else:
                if stretch:
                    ofh.write("fixedStep chrom=%s span=1 step=1 start=%d\n"
                              % (chrom, stretchStart + 1))
                    for v in stretch:
                        ofh.write("%s\n" % v)
                    ofh.write("\n")
                stretch = []
        if stretch:
            ofh.write("fixedStep chrom=%s span=1 step=1 start=%d\n"
                      % (chrom, stretchStart + 1))
            for v in stretch:
                ofh.write("%s\n" % v)
            ofh.write("\n")


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: clinPredToWig.py chrom.sizes inputFile\n")
        sys.stderr.write("  Writes a.wig, c.wig, g.wig, t.wig in current directory.\n")
        sys.exit(1)

    chromSizesFname = sys.argv[1]
    inFname = sys.argv[2]
    chromSizes = parseChromSizes(chromSizesFname)

    outFhs = {i: open(name, "w") for i, name in enumerate(OUT_NAMES)}

    lastChrom = None
    arrays = None

    for line in open(inFname):
        if line.startswith("Chr\t"):
            continue
        chromName, startStr, ref, alt, scoreStr = line.rstrip("\n").split("\t")
        ucscChrom = "chr" + chromName

        if ucscChrom != lastChrom:
            if lastChrom is not None and arrays is not None:
                writeChrom(lastChrom, arrays, outFhs)
            if ucscChrom not in chromSizes:
                sys.stderr.write("warning: %s not in chrom.sizes, skipping\n" % ucscChrom)
                arrays = None
                lastChrom = ucscChrom
                continue
            sys.stderr.write("loading %s\n" % ucscChrom)
            arrays = initArrays(chromSizes[ucscChrom])
            lastChrom = ucscChrom

        if arrays is None:
            continue

        if alt not in NUC_IDX:
            continue

        pos = int(startStr) - 1
        arrays[NUC_IDX[alt]][pos] = float(scoreStr)

    if lastChrom is not None and arrays is not None:
        writeChrom(lastChrom, arrays, outFhs)

    for f in outFhs.values():
        f.close()


if __name__ == "__main__":
    main()
