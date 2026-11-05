#!/usr/bin/env python3
"""Find insertions whose inserted sequence does not align to the human genome.

Reads bwa mem SAM output on stdin, for the numbered sequences in uniq.fa. A
sequence counts as human if its primary alignment covers at least MINCOV of
its length (soft/hard clips do not count) with at most MAXDIV mismatches and
gap bases (NM) per aligned base. Then writes, for every record of ins.tsv
(CHROM POS REF ALT, ALT = REF + inserted sequence) whose inserted sequence is
not human and that is not in gnomadIns.tsv (same columns, the insertions of
gnomAD v4.1 genomes): CHROM POS REF ALT 1, for bcftools annotate. Counts go to
stderr.

Usage:
    bwa mem ... | sparkWgs45kNonHumanIns.py uniq.fa ins.tsv gnomadIns.tsv > nonHuman.tsv
"""

import re
import sys
from collections import Counter

MINCOV = 0.8
MAXDIV = 0.1
CIGAR_RE = re.compile(r"(\d+)([MIDNSHP=X])")


def lenBin(n):
    for hi, lab in ((20, "10-19"), (30, "20-29"), (50, "30-49"), (100, "50-99"), (150, "100-149")):
        if n < hi:
            return lab
    return ">=150"


def main():
    faName, insName, gnomadName = sys.argv[1:4]
    seqLen = {}
    name = None
    for line in open(faName):
        if line.startswith(">"):
            name = line[1:].strip()
        else:
            seqLen[name] = len(line.strip())

    human = set()
    for line in sys.stdin:
        if line.startswith("@"):
            continue
        f = line.rstrip("\n").split("\t")
        flag = int(f[1])
        if flag & 0x904:        # unmapped, secondary or supplementary
            continue
        aligned = sum(int(n) for n, op in CIGAR_RE.findall(f[5]) if op in "MI=X")
        nm = 0
        for tag in f[11:]:
            if tag.startswith("NM:i:"):
                nm = int(tag[5:])
        if aligned >= MINCOV * seqLen[f[0]] and nm <= MAXDIV * aligned:
            human.add(f[0])

    stats = Counter()
    for name, n in seqLen.items():
        stats[(lenBin(n), name in human)] += 1
    del seqLen

    # map sequence -> human?, streaming through uniq.fa again
    nonHumanSeqs = set()
    name = None
    for line in open(faName):
        if line.startswith(">"):
            name = line[1:].strip()
        elif name not in human:
            nonHumanSeqs.add(line.strip())
    del human

    # insertions that gnomAD has too are real, even if the reference lacks them
    inGnomad = set(line.rstrip("\n") for line in open(gnomadName))

    nRec = nFlag = nGnomad = 0
    out = sys.stdout
    for line in open(insName):
        line = line.rstrip("\n")
        chrom, pos, ref, alt = line.split("\t")
        nRec += 1
        if alt[len(ref):] in nonHumanSeqs:
            if line in inGnomad:
                nGnomad += 1
                continue
            out.write("%s\t1\n" % line)
            nFlag += 1

    sys.stderr.write("distinct inserted sequences by length: human / not human\n")
    for lab in ("10-19", "20-29", "30-49", "50-99", "100-149", ">=150"):
        h, nh = stats[(lab, True)], stats[(lab, False)]
        if h + nh:
            sys.stderr.write("  %s\t%d\t%d\t(%.1f%% not human)\n" % (lab, h, nh, 100.0 * nh / (h + nh)))
    sys.stderr.write("insertion records tested: %d, not human but in gnomAD (kept): %d, "
                     "flagged NonHumanIns: %d\n" % (nRec, nGnomad, nFlag))


if __name__ == "__main__":
    main()
