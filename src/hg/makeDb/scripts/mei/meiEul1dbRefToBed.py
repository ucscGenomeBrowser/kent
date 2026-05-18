#!/usr/bin/env python3
"""Convert euL1db ReferenceL1HS.txt to a hg19 BED9+ for the meiEul1dbRef track.

These are L1-HS copies already present in the human reference genome —
not insertion polymorphisms.
"""

import argparse
import os
import sys
from collections import defaultdict

# Okabe-Ito colors by L1HS subgroup
COLORS = {
    "L1HS-Ta":    "0,114,178",      # blue
    "L1HS-PreTa": "230,159,0",      # orange
    "L1HS-undef": "153,153,153",    # grey
}


def open_tab(path):
    with open(path) as fh:
        for line in fh:
            if not line.strip() or line.startswith("#"):
                continue
            yield line.rstrip("\n").split("\t")


def load_chrom_sizes(path):
    sizes = {}
    with open(path) as fh:
        for line in fh:
            chrom, size = line.rstrip().split("\t")[:2]
            sizes[chrom] = int(size)
    return sizes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default="/hive/data/genomes/hg38/bed/mei/eul1db",
                    help="Directory containing euL1db .txt tables")
    ap.add_argument("--chrom-sizes", default="/hive/data/genomes/hg19/chrom.sizes")
    ap.add_argument("-o", "--out", required=True, help="Output BED9+ file")
    args = ap.parse_args()

    sizes = load_chrom_sizes(args.chrom_sizes)

    n_in = 0
    n_out = 0
    n_skip_chrom = 0
    n_skip_range = 0
    dropped_chroms = defaultdict(int)
    with open(args.out, "w") as out:
        for cols in open_tab(os.path.join(args.src, "ReferenceL1HS.txt")):
            n_in += 1
            if len(cols) < 9:
                continue
            chrom, start_s, stop_s, family, strand, ref_start, ref_stop, \
                integrity, sub_group = cols[:9]
            chrom = "chr" + chrom
            if chrom not in sizes:
                dropped_chroms[chrom] += 1
                n_skip_chrom += 1
                continue
            try:
                start_1 = int(start_s)
                stop_1  = int(stop_s)
            except ValueError:
                continue
            bed_start = max(0, start_1 - 1)
            bed_end   = stop_1
            if bed_end <= bed_start:
                bed_end = bed_start + 1
            if bed_end > sizes[chrom]:
                n_skip_range += 1
                continue
            if strand not in ("+", "-"):
                strand = "."
            rgb = COLORS.get(sub_group, "153,153,153")
            try:
                rs = int(ref_start) if ref_start not in (".", "") else 0
                re_ = int(ref_stop) if ref_stop not in (".", "") else 0
            except ValueError:
                rs = re_ = 0
            elem_len = bed_end - bed_start
            out.write("\t".join([
                chrom,
                str(bed_start),
                str(bed_end),
                sub_group if sub_group else "L1HS",
                "0",
                strand,
                str(bed_start),
                str(bed_end),
                rgb,
                family if family else "L1HS",
                sub_group if sub_group else "",
                integrity if integrity else "",
                str(rs),
                str(re_),
                str(elem_len),
            ]) + "\n")
            n_out += 1

    print(f"Reference L1HS read: {n_in}", file=sys.stderr)
    print(f"Reference L1HS written: {n_out}", file=sys.stderr)
    print(f"Skipped (chrom not in hg19 chrom.sizes): {n_skip_chrom}", file=sys.stderr)
    if dropped_chroms:
        for c, n in sorted(dropped_chroms.items()):
            print(f"    {c}: {n}", file=sys.stderr)
    print(f"Skipped (end beyond chrom): {n_skip_range}", file=sys.stderr)


if __name__ == "__main__":
    main()
