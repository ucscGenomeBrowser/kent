#!/usr/bin/env python3
"""Convert gnomAD v4.1.1 MPC TSV into four wig files, one per ALT base.

Input format (TSV, with header):
    locus<TAB>alleles<TAB>transcript<TAB>mpc
    chr1:65568<TAB>["A","C"]<TAB>ENST00000641515<TAB>8.8102e-01

Writes a.wig, c.wig, g.wig, t.wig in the current directory. At positions
where the variant ALT matches the file's nucleotide, the MPC score is
emitted; otherwise 0.0. If the same (chrom, pos, alt) appears in multiple
transcripts with different scores, the maximum is kept.
"""

import sys
import gzip
import json

def readRows(infile):
    opener = gzip.open if infile.endswith((".gz", ".bgz")) else open
    with opener(infile, "rt") as f:
        header = f.readline()
        if not header.startswith("locus"):
            sys.exit("unexpected header: " + header)
        for line in f:
            if not line.strip():
                continue
            fields = line.rstrip("\n").split("\t")
            locus, alleles_str, transcript, mpc = fields
            chrom, pos = locus.split(":")
            pos = int(pos)
            alleles = json.loads(alleles_str)
            ref, alt = alleles[0], alleles[1]
            if ref not in "ACGT" or alt not in "ACGT":
                continue
            yield chrom, pos, ref, alt, float(mpc)

def chunks(infile):
    """Yield (chrom, firstPos, [[A,C,G,T], ...]) for contiguous runs."""
    firstChrom = None
    firstPos = None
    lastChrom = None
    lastPos = None
    values = []

    for chrom, pos, ref, alt, mpc in readRows(infile):
        if lastChrom is None:
            firstChrom, firstPos = chrom, pos
            values = [[0.0, 0.0, 0.0, 0.0]]
        elif chrom != lastChrom or pos - lastPos > 1:
            yield firstChrom, firstPos, values
            firstChrom, firstPos = chrom, pos
            values = [[0.0, 0.0, 0.0, 0.0]]
        elif pos != lastPos:
            values.append([0.0, 0.0, 0.0, 0.0])
        # else same pos: multiple transcripts, fold into current slot

        idx = "ACGT".find(alt)
        if mpc > values[-1][idx]:
            values[-1][idx] = mpc

        lastChrom, lastPos = chrom, pos

    if values:
        yield firstChrom, firstPos, values

def main():
    if len(sys.argv) != 2:
        sys.exit("usage: gnomadMpcToWig.py input.tsv.{gz,bgz}")

    infile = sys.argv[1]
    outFhs = {base: open(base.lower() + ".wig", "w") for base in "ACGT"}

    for chrom, firstPos, nuclValues in chunks(infile):
        if not nuclValues:
            continue
        for fh in outFhs.values():
            fh.write("fixedStep chrom=%s span=1 step=1 start=%d\n" % (chrom, firstPos))
        for vals in nuclValues:
            for i, base in enumerate("ACGT"):
                outFhs[base].write("%g\n" % vals[i])

    for fh in outFhs.values():
        fh.close()

if __name__ == "__main__":
    main()
