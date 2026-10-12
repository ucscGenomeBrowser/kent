#!/usr/bin/env python3
"""Convert Table 1 of Monk et al 2018 (tab-separated copy) to a bed9+6 file in hg19 coordinates.

The table gives 1-based start and end, so the bed start is start-1. Cleanups of the
PMC web copy: thousands separators are removed from the numbers and from the LRG ids
(LRG_1,034 is LRG_1034), the en dash in VTRNA2-1 and the prime in ZNF597:3' DMR become
ASCII, and M/P is spelled out. Colors are the strong red and blue of the Kim and Akbari tracks."""
import sys

origin = {"M": ("Maternal", "220,20,20"), "P": ("Paternal", "0,60,200")}

def main():
    inFn, outFn = sys.argv[1:3]
    n = 0
    with open(inFn) as fh, open(outFn, "w") as out:
        next(fh)
        for line in fh:
            f = line.rstrip("\n").split("\t")
            name, chrom, start, end, cpgs, org, germ, lrg, aliases = f
            name = name.replace("–", "-").replace("′", "'")
            start = int(start.replace(",", "")); end = int(end.replace(",", ""))
            lrg = lrg.replace(",", "")
            o, rgb = origin[org]
            pos = "chr%s:%d-%d" % (chrom, start, end)
            out.write("\t".join(["chr" + chrom, str(start - 1), str(end), name, "0", ".",
                                 str(start - 1), str(end), rgb, o, cpgs, germ, lrg, aliases, pos]) + "\n")
            n += 1
    sys.stderr.write("%d DMRs written\n" % n)

main()
