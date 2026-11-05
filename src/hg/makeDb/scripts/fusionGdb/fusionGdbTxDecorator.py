#!/usr/bin/env python3
"""Make the decorator bed for fusionGdbTx: one ArrLeft or ArrRight glyph per feature, on the
breakpoint end of the kept transcript part. The arrow points in the direction of
transcription, given by the strand field: towards the breakpoint for the 5' partner, away
from it for the 3' partner.
The decoration's mouseover is the mouseover of the feature, so it is not hidden by the glyph.
Usage: fusionGdbTxDecorator.py fusionGdbTx.bed fusionGdbTxDecorator.bed"""

import sys

barColor = "0,0,0"
fillColor = "232,89,12,128"   # #e8590c, 50% transparent
sideCol = 20

def main():
    inFname, outFname = sys.argv[1:]
    rows = []
    for line in open(inFname):
        row = line.rstrip("\n").split("\t")
        chrom, start, end, name, strand = row[0], int(row[1]), int(row[2]), row[3], row[5]
        side = row[sideCol]
        # the 5' partner keeps the transcript up to the breakpoint, the 3' partner from it
        if (side == "5'") == (strand == "+"):
            glyph = "ArrRight"
        else:
            glyph = "ArrLeft"
        decoratedItem = "%s:%d-%d:%s" % (chrom, start, end, name)
        rows.append((chrom, start, end, "breakpoint", 0, strand, start, end, barColor,
                     1, end-start, 0, decoratedItem, "glyph", fillColor, glyph,
                     "Breakpoint of: " + row[-1]))
    rows.sort(key=lambda r: (r[0], r[1]))
    with open(outFname, "w") as ofh:
        for r in rows:
            ofh.write("\t".join([str(x) for x in r]) + "\n")

main()
