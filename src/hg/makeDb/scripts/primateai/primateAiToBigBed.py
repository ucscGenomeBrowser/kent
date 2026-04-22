#!/usr/bin/env python3
"""
Convert PrimateAI-3D.hg38.txt.gz to a BED file for bigBed conversion.
Input positions are 1-based, output BED is 0-based.
Colors: red for pathogenic, blue for benign.
"""

import gzip
import sys
import os

def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: primateAiToBigBed.py <input.txt.gz> <output.bed>\n")
        sys.exit(1)
    inPath, outPath = sys.argv[1], sys.argv[2]

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
            # hg19 source has some rows missing the refseq column (11 fields instead of 12)
            if len(fields) >= 12:
                refSeq = fields[10]
                prediction = fields[11]
            else:
                refSeq = ""
                prediction = fields[10]

            chromStart = pos - 1  # convert to 0-based
            chromEnd = pos
            name = f"{ref}>{alt}"
            aaChange = f"{refAa}>{altAa}"
            score = int(round(percentile * 1000))
            rgb = "200,0,0" if prediction == "pathogenic" else "0,0,200"
            color = "#c80000" if prediction == "pathogenic" else "#0000c8"

            mouseOver = (f"<b>Var</b>: {ref}>{alt}<br>"
                         f"<b>AA</b>: {aaChange}<br>"
                         f"<b>Score</b>: {scorePai:.3f}<br>"
                         f"<b>Perc</b>: {percentile:.3f}<br>"
                         f"<b>Pred</b>: <span style=\"color:{color}\">{prediction}</span>")

            out.write(f"{chrom}\t{chromStart}\t{chromEnd}\t{name}\t{score}\t"
                       f".\t{chromStart}\t{chromEnd}\t{rgb}\t"
                       f"{aaChange}\t{ref}\t{alt}\t{gene}\t{refSeq}\t"
                       f"{scorePai:.3f}\t{percentile:.3f}\t"
                       f"{prediction}\t{mouseOver}\n")
            count += 1
            if count % 10000000 == 0:
                print(f"  {count} variants processed...", file=sys.stderr)

    print(f"  {count} variants written to {outPath}", file=sys.stderr)
    print("Done. Now sort and run bedToBigBed.", file=sys.stderr)

if __name__ == "__main__":
    main()
