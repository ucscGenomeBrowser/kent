#!/usr/bin/env python3
"""Convert danRer4 bacEndPairs linked-features export to BED12 for liftOver.

Input (tab-separated): chrom chromStart chromEnd name score strand lfCount lfStarts lfSizes
Output (BED12): chrom chromStart chromEnd name score strand thickStart thickEnd itemRgb blockCount blockSizes blockStarts
"""
import sys

for line in sys.stdin:
    fields = line.rstrip("\n").split("\t")
    chrom, start, end, name, score, strand = fields[0:6]
    start = int(start)
    end = int(end)
    lfCount = int(fields[6])
    lfStarts = [int(x) for x in fields[7].rstrip(",").split(",")]
    lfSizes = [int(x) for x in fields[8].rstrip(",").split(",")]

    # BED12 blockStarts are relative to chromStart
    blockStarts = [s - start for s in lfStarts]
    blockSizes = lfSizes

    print("\t".join([
        chrom, str(start), str(end), name, score, strand,
        str(start), str(end),   # thickStart, thickEnd
        "0",                    # itemRgb
        str(lfCount),
        ",".join(str(s) for s in blockSizes) + ",",
        ",".join(str(s) for s in blockStarts) + ","
    ]))
