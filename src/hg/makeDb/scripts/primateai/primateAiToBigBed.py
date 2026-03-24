#!/usr/bin/env python3
"""
Convert PrimateAI-3D.hg38.txt.gz to a BED file for bigBed conversion.
Input positions are 1-based, output BED is 0-based.
Colors: red for pathogenic, blue for benign.
"""

import gzip
import sys
import os

INFILE = "PrimateAI-3D.hg38.txt.gz"
OUTFILE = "primateAi.bed"

def main():
    inPath = INFILE
    outPath = OUTFILE

    print(f"Reading {inPath}...", file=sys.stderr)
    count = 0
    with gzip.open(inPath, "rt") as fh, open(outPath, "w") as out:
        header = fh.readline()  # skip header
        for line in fh:
            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])  # 1-based
            ref = fields[2]
            alt = fields[3]
            gene = fields[4]
            refAa = fields[6]
            altAa = fields[7]
            scorePai = float(fields[8])
            percentile = float(fields[9])
            refSeq = fields[10]
            prediction = fields[11]

            chromStart = pos - 1  # convert to 0-based
            chromEnd = pos
            name = f"{refAa}>{altAa}"
            score = int(round(percentile * 1000))
            rgb = "200,0,0" if prediction == "pathogenic" else "0,0,200"

            mouseOver = f"{ref}>{alt} {name} score={scorePai:.3f} pct={percentile:.3f} ({prediction})"

            out.write(f"{chrom}\t{chromStart}\t{chromEnd}\t{name}\t{score}\t"
                       f"+\t{chromStart}\t{chromEnd}\t{rgb}\t"
                       f"{ref}\t{alt}\t{gene}\t{refSeq}\t{scorePai:.3f}\t{percentile:.3f}\t"
                       f"{prediction}\t{mouseOver}\n")
            count += 1
            if count % 10000000 == 0:
                print(f"  {count} variants processed...", file=sys.stderr)

    print(f"  {count} variants written to {outPath}", file=sys.stderr)
    print("Done. Now sort and run bedToBigBed.", file=sys.stderr)

if __name__ == "__main__":
    main()
