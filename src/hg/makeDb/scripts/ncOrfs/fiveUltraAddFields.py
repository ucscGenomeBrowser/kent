#!/usr/bin/env python3
"""
Append gene, rank and kozak fields to the 5ULTRA bed12+2 from addIntrons.py, giving
the bed12+5 that fiveUltraUorfs.as describes.

The rank total is not in the addIntrons output, as maneUorfsToBed.py shortens the
name to GENE_N, so it is read back from the original 5ULTRA BED, whose names are
GENE|Type|RANK_TOTAL|Kozak. The two files are joined on chrom, start, end, strand
and the short name; addIntrons.py does not change the uORF start and end.

  gene  - gene symbol (GENE from GENE_N)
  rank  - "N of M": the uORF's rank within the gene, out of M uORFs in that gene
  kozak - from the score: 1000=Strong, 700=Moderate, 300=Weak, 100=NA (no context)

Usage: fiveUltraAddFields.py uORFs.MANE.hg38.bed uORFs.MANE.bed12plus2 > uORFs.MANE.bed12plus5
"""
import sys

KOZAK = {1000: "Strong", 700: "Moderate", 300: "Weak", 100: "NA"}

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    origFname, inFname = sys.argv[1:]

    rankTotal = {}
    for line in open(origFname):
        if line.startswith("track "):
            continue
        f = line.rstrip("\n").split("\t")
        gene, _, rank, _ = f[3].split("|")
        pos, total = rank.split("_")
        key = (f[0], f[1], f[2], f[5], f"{gene}_{pos}")
        assert key not in rankTotal, key
        rankTotal[key] = (gene, pos, total)

    for line in open(inFname):
        f = line.rstrip("\n").split("\t")
        gene, pos, total = rankTotal[(f[0], f[1], f[2], f[5], f[3])]
        kozak = KOZAK[int(f[4])]
        print("\t".join(f + [gene, f"{pos} of {total}", kozak]))

main()
