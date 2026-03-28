#!/usr/bin/env python3
"""
Convert promoterAI_tss500.tsv.gz to 4 bedGraph files (one per alt allele)
and a BED file for overlapping positions where transcripts disagree on score.

For bigWig: picks the score with the largest absolute value per (chr, pos, alt).
For overlap bigBed: outputs all transcript-level scores where they differ.

Input positions are 1-based, output is 0-based.
"""

import gzip
import sys
import os
from collections import defaultdict

INFILE = "promoterAI_tss500.tsv.gz"

def main():
    print("Pass 1: Reading input and collecting scores per (chr, pos, alt)...", file=sys.stderr)

    # For each (chr, pos, alt), store list of (score, gene, transcript_id)
    variants = defaultdict(list)
    count = 0
    with gzip.open(INFILE, "rt") as fh:
        fh.readline()  # skip header
        for line in fh:
            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])   # 1-based
            alt = fields[3]
            gene = fields[4]
            txId = fields[6]
            score = float(fields[9])

            key = (chrom, pos, alt)
            variants[key].append((score, gene, txId))
            count += 1
            if count % 50000000 == 0:
                print(f"  {count} rows read...", file=sys.stderr)

    print(f"  {count} total rows, {len(variants)} unique (chr, pos, alt) combos.", file=sys.stderr)

    # Pass 2: Write bedGraph files and overlap BED
    print("Pass 2: Writing bedGraph and overlap BED files...", file=sys.stderr)

    bgFiles = {}
    for alt in "ACGT":
        bgFiles[alt] = open(f"promoterAi_{alt}.bedGraph", "w")

    overlapOut = open("promoterAi_overlaps.bed", "w")
    overlapCount = 0
    bgCount = 0

    for (chrom, pos, alt), entries in sorted(variants.items()):
        chromStart = pos - 1  # convert to 0-based
        chromEnd = pos

        # Pick score with largest absolute value for bigWig
        bestScore = max(entries, key=lambda x: abs(x[0]))[0]
        bgFiles[alt].write(f"{chrom}\t{chromStart}\t{chromEnd}\t{bestScore:.4f}\n")
        bgCount += 1

        # Check if scores differ across transcripts
        uniqueScores = set(e[0] for e in entries)
        if len(uniqueScores) > 1:
            # Merge into one row: comma-separated transcripts and scores
            gene = entries[0][1]  # use first gene name
            txList = ",".join(e[2] for e in entries)
            scoreList = ",".join(f"{e[0]:.4f}" for e in entries)
            minScore = min(e[0] for e in entries)
            maxScore = max(e[0] for e in entries)
            # bed score from best (max abs) score
            bedScore = int(round((bestScore + 1) / 2 * 1000))
            bedScore = max(0, min(1000, bedScore))
            # Color: red if best score positive, blue if negative
            rgb = "200,0,0" if bestScore > 0 else "0,0,200"
            scoreDiff = maxScore - minScore
            mouseOver = f"{alt} {gene} score range {minScore:.4f} to {maxScore:.4f} (diff {scoreDiff:.4f})"
            overlapOut.write(f"{chrom}\t{chromStart}\t{chromEnd}\t{gene}\t{bedScore}\t"
                             f"+\t{chromStart}\t{chromEnd}\t{rgb}\t"
                             f"{alt}\t{txList}\t{scoreList}\t{scoreDiff:.4f}\t{mouseOver}\n")
            overlapCount += 1

    for f in bgFiles.values():
        f.close()
    overlapOut.close()

    print(f"  {bgCount} positions written to 4 bedGraph files.", file=sys.stderr)
    print(f"  {overlapCount} overlap entries written to promoterAi_overlaps.bed", file=sys.stderr)
    print("Done. Now sort bedGraphs and convert to bigWig, sort BED and convert to bigBed.", file=sys.stderr)

if __name__ == "__main__":
    main()
