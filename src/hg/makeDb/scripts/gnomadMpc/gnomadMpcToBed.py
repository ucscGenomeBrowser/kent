#!/usr/bin/env python3
"""Convert gnomAD v4.1.1 MPC TSV into a BED9+ file of multi-transcript variants.

Input TSV (with header):
    locus<TAB>alleles<TAB>transcript<TAB>mpc
    chr1:65568<TAB>["A","C"]<TAB>ENST00000641515<TAB>8.8102e-01

Only variants scored against more than one transcript are emitted (single-
transcript variants are already fully represented by the companion bigWigs).
Per-transcript scores are folded into comma-separated lstring fields so the
track-detail page can show the full breakdown. The BED score column uses the
max MPC across transcripts, scaled to the 0-1000 range (MPC 6 -> ~1000).

Input must already be sorted so that all rows for the same (chrom, pos) are
grouped together; alts/transcripts within a position may appear in any order.
Output is written unsorted-within-pos-but-emitted-in-input-order; pipe through
`sort -k1,1 -k2,2n` before bedToBigBed.
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
            locus, alleles_str, transcript, mpc = line.rstrip("\n").split("\t")
            chrom, pos = locus.split(":")
            alleles = json.loads(alleles_str)
            ref, alt = alleles[0], alleles[1]
            if ref not in "ACGT" or alt not in "ACGT":
                continue
            yield chrom, int(pos), ref, alt, transcript, float(mpc)

def mpcColor(score):
    """Map MPC score to an RGB string (gray -> yellow -> orange -> red)."""
    if score < 1:
        return "200,200,200"
    if score < 2:
        return "255,200,100"
    if score < 3:
        return "255,150,50"
    return "200,0,0"

def emit(out, chrom, pos, ref, alt_groups):
    start = pos - 1
    end = pos
    for alt in sorted(alt_groups):
        items = alt_groups[alt]
        if len(items) < 2:
            continue
        max_mpc = max(score for _, score in items)
        transcripts = ",".join(tx for tx, _ in items)
        scores = ",".join("%g" % score for _, score in items)
        name = "%s>%s" % (ref, alt)
        score_int = min(1000, int(max_mpc * 166))
        color = mpcColor(max_mpc)
        out.write("\t".join([
            chrom, str(start), str(end), name,
            str(score_int), ".",
            str(start), str(end), color,
            ref, alt, "%g" % max_mpc,
            transcripts, scores, str(len(items))
        ]) + "\n")

def main():
    if len(sys.argv) != 3:
        sys.exit("usage: gnomadMpcToBed.py input.tsv.{gz,bgz} output.bed")

    infile, outfile = sys.argv[1:]
    out = open(outfile, "w")

    cur_chrom = None
    cur_pos = None
    cur_ref = None
    alt_groups = {}

    for chrom, pos, ref, alt, transcript, mpc in readRows(infile):
        if (chrom, pos) != (cur_chrom, cur_pos):
            if cur_chrom is not None:
                emit(out, cur_chrom, cur_pos, cur_ref, alt_groups)
            cur_chrom, cur_pos, cur_ref = chrom, pos, ref
            alt_groups = {}
        alt_groups.setdefault(alt, []).append((transcript, mpc))

    if alt_groups:
        emit(out, cur_chrom, cur_pos, cur_ref, alt_groups)
    out.close()

if __name__ == "__main__":
    main()
