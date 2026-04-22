#!/usr/bin/env python3
"""
Convert promoterAI_tss500.tsv.gz to 4 bedGraph files (one per alt allele)
and a BED file for overlapping positions where transcripts disagree on score.

For bigWig: picks the score with the largest absolute value per (chr, pos, alt).
For overlap bigBed: outputs all transcript-level scores where they differ.

Input is assumed sorted by (chrom, pos) (and rows for the same (chrom, pos) are
therefore consecutive). Processing is streaming: memory use is proportional to
the number of transcripts at a single position, not the whole file.

Input positions are 1-based, output is 0-based.
Input strand is encoded as "1" (plus) or "-1" (minus).
"""

import gzip
import sys
from collections import defaultdict

INFILE = "promoterAI_tss500.tsv.gz"

def strandStr(s):
    return "+" if s == "1" else "-" if s == "-1" else "."

def flushPosition(chrom, pos, byAlt, bgFiles, overlapOut, counters):
    chromStart = pos - 1
    chromEnd = pos
    for alt, entries in byAlt.items():
        bestScore = max(entries, key=lambda x: abs(x[0]))[0]
        bgFiles[alt].write(f"{chrom}\t{chromStart}\t{chromEnd}\t{bestScore:.4f}\n")
        counters["bg"] += 1

        uniqueScores = set(e[0] for e in entries)
        if len(uniqueScores) > 1:
            # Unique gene names in order of first appearance
            genes = []
            for e in entries:
                if e[1] not in genes:
                    genes.append(e[1])
            geneStr = ",".join(genes)

            txList = ",".join(e[2] for e in entries)
            scoreList = ",".join(f"{e[0]:.4f}" for e in entries)
            strandList = ",".join(e[3] for e in entries)

            strandSet = set(e[3] for e in entries)
            bedStrand = strandSet.pop() if len(strandSet) == 1 else "."

            minScore = min(e[0] for e in entries)
            maxScore = max(e[0] for e in entries)
            scoreDiff = maxScore - minScore
            bedScore = int(round(abs(bestScore) * 1000))
            bedScore = max(0, min(1000, bedScore))
            rgb = "200,0,0" if bestScore > 0 else "0,0,200"
            mouseOver = (f"{alt} {geneStr} score range {minScore:.4f} to "
                         f"{maxScore:.4f} (diff {scoreDiff:.4f})")
            overlapOut.write(f"{chrom}\t{chromStart}\t{chromEnd}\t{geneStr}\t{bedScore}\t"
                             f"{bedStrand}\t{chromStart}\t{chromEnd}\t{rgb}\t"
                             f"{alt}\t{txList}\t{scoreList}\t{strandList}\t"
                             f"{scoreDiff:.4f}\t{mouseOver}\n")
            counters["overlap"] += 1

def main():
    print("Streaming input, writing bedGraph and overlap BED files...", file=sys.stderr)

    bgFiles = {alt: open(f"promoterAi_{alt}.bedGraph", "w") for alt in "ACGT"}
    overlapOut = open("promoterAi_overlaps.bed", "w")
    counters = {"bg": 0, "overlap": 0}

    prevChrom = None
    prevPos = None
    byAlt = defaultdict(list)
    count = 0

    with gzip.open(INFILE, "rt") as fh:
        fh.readline()  # skip header
        for line in fh:
            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])
            alt = fields[3]
            gene = fields[4]
            txId = fields[6]
            strand = strandStr(fields[7])
            score = float(fields[9])

            if chrom != prevChrom or pos != prevPos:
                if prevChrom is not None:
                    flushPosition(prevChrom, prevPos, byAlt, bgFiles, overlapOut, counters)
                byAlt = defaultdict(list)
                prevChrom, prevPos = chrom, pos

            byAlt[alt].append((score, gene, txId, strand))
            count += 1
            if count % 50000000 == 0:
                print(f"  {count} rows read...", file=sys.stderr)

    if prevChrom is not None:
        flushPosition(prevChrom, prevPos, byAlt, bgFiles, overlapOut, counters)

    for f in bgFiles.values():
        f.close()
    overlapOut.close()

    print(f"  {count} total rows read.", file=sys.stderr)
    print(f"  {counters['bg']} positions written to 4 bedGraph files.", file=sys.stderr)
    print(f"  {counters['overlap']} overlap entries written to promoterAi_overlaps.bed",
          file=sys.stderr)

if __name__ == "__main__":
    main()
