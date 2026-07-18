#!/usr/bin/env python3
# Left-normalize deletions against a target 2bit, using the same case-insensitive
# base-slide rule as chainInDel (-t2bit): slide the deleted interval left while
# seq[start-1] == seq[end-1].  Unbounded (stops only at the chromosome start).
# Emits a canonical key so two catalogs of deletions can be compared exactly.
# Redmine #35415
#
# Usage: leftNormDel.py <2bit> <in.tsv> <annotCol> > out.tsv
#   in.tsv columns: chrom<TAB>start<TAB>end<TAB>...   (0-based half-open)
#   annotCol: 1-based column to carry through as an annotation (e.g. score); 0 = none
# Output columns: chrom  normStart  normEnd  length  annot

import sys, subprocess

twoBit, inFile = sys.argv[1], sys.argv[2]
annotCol = int(sys.argv[3])

# group input rows by chromosome
byChrom = {}
with open(inFile) as f:
    for line in f:
        t = line.rstrip("\n").split("\t")
        chrom, start, end = t[0], int(t[1]), int(t[2])
        annot = t[annotCol-1] if annotCol > 0 else "."
        byChrom.setdefault(chrom, []).append((start, end, annot))

def loadSeq(chrom):
    p = subprocess.run(["twoBitToFa", "-seq=" + chrom, twoBit, "stdout"],
                       capture_output=True, text=True)
    if p.returncode != 0:
        return None
    lines = p.stdout.split("\n")
    return "".join(lines[1:]).upper()   # drop header, concat, uppercase

out = sys.stdout
for chrom in sorted(byChrom):
    seq = loadSeq(chrom)
    if seq is None:
        continue
    n = len(seq)
    for (start, end, annot) in byChrom[chrom]:
        s, e = start, end
        # slide left while base leaving the left edge equals base leaving the deletion
        while s > 0 and e-1 < n and seq[s-1] == seq[e-1]:
            s -= 1; e -= 1
        out.write("%s\t%d\t%d\t%d\t%s\n" % (chrom, s, e, e-s, annot))
