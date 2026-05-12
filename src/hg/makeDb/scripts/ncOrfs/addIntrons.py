#!/usr/bin/env python3
"""
Add exon/intron block structure to utrAnnotUorfs (bed9+1) by looking up matches
in the other ncORF bigBed tracks by (chrom, start, end, strand).

The utrAnnotUorfs source is a bed9+1 with single-span features — no introns.
For each feature, we search the other ncORF bigBeds for a feature with
identical (chrom, start, end, strand). If exactly one or more match, we copy
the block structure from the multi-block candidate with the most blocks, and
record all source databases that produced matches in the new intronsSource
field. If no match is found, the output is a single block and intronsSource
is set to "none".

Output is bed12 + uorfType + intronsSource (14 fields).
"""

import argparse
import subprocess
import sys
from collections import defaultdict


def read_other_bb(bb_path, source_name):
    """Yield (key, (blockCount, blockSizes, chromStarts, source_name)) for every
    multi-block feature in a bigBed."""
    p = subprocess.Popen(["bigBedToBed", bb_path, "stdout"],
                         stdout=subprocess.PIPE, text=True)
    n = 0
    n_multi = 0
    for line in p.stdout:
        fields = line.rstrip("\n").split("\t")
        if len(fields) < 12:
            continue
        n += 1
        chrom, start, end = fields[0], int(fields[1]), int(fields[2])
        strand = fields[5]
        block_count = int(fields[9])
        block_sizes = fields[10]
        chrom_starts = fields[11]
        if block_count <= 1:
            continue
        n_multi += 1
        key = (chrom, start, end, strand)
        yield key, (block_count, block_sizes, chrom_starts, source_name)
    p.wait()
    sys.stderr.write(f"[addIntrons]   {source_name}: {n} features, "
                     f"{n_multi} multi-block\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="inBed", required=True,
                    help="input utrAnnotUorfs BED (bed9+1)")
    ap.add_argument("--out", dest="outBed", required=True,
                    help="output BED12 + uorfType + intronsSource")
    ap.add_argument("--source", action="append", required=True,
                    help="NAME:PATH to another ncORF bigBed (repeatable)")
    ap.add_argument("--report",
                    help="optional path to write a summary TSV")
    args = ap.parse_args()

    # Load all donors
    lookup = defaultdict(list)
    for spec in args.source:
        if ":" not in spec:
            sys.exit(f"--source must be NAME:PATH, got {spec!r}")
        name, path = spec.split(":", 1)
        sys.stderr.write(f"[addIntrons] loading {name} from {path}\n")
        for key, val in read_other_bb(path, name):
            lookup[key].append(val)

    sys.stderr.write(f"[addIntrons] {len(lookup)} unique coord keys with "
                     f"multi-block donors\n")

    # Process utrAnnotUorfs
    matched = 0
    unmatched = 0
    by_source = defaultdict(int)
    with open(args.inBed) as fh, open(args.outBed, "w") as out:
        for line in fh:
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 10:
                continue
            chrom = fields[0]
            start = int(fields[1])
            end = int(fields[2])
            name = fields[3]
            score = fields[4]
            strand = fields[5]
            thick_start = fields[6]
            thick_end = fields[7]
            item_rgb = fields[8]
            uorf_type = fields[9]

            key = (chrom, start, end, strand)
            hits = lookup.get(key)
            if hits:
                # Pick the donor with the most blocks; tie-break by source name
                # for determinism
                best = max(hits, key=lambda v: (v[0], v[3]))
                block_count, block_sizes, chrom_starts, _ = best
                sources = ",".join(sorted({h[3] for h in hits}))
                matched += 1
                by_source[sources] += 1
            else:
                size = end - start
                block_count = 1
                block_sizes = f"{size},"
                chrom_starts = "0,"
                sources = "none"
                unmatched += 1

            bed12 = [chrom, str(start), str(end), name, score, strand,
                     thick_start, thick_end, item_rgb,
                     str(block_count), block_sizes, chrom_starts]
            out.write("\t".join(bed12 + [uorf_type, sources]) + "\n")

    sys.stderr.write(f"[addIntrons] matched={matched} unmatched={unmatched}\n")
    for src, cnt in sorted(by_source.items(), key=lambda x: -x[1]):
        sys.stderr.write(f"[addIntrons]   {src}: {cnt}\n")

    if args.report:
        with open(args.report, "w") as r:
            r.write(f"matched\t{matched}\n")
            r.write(f"unmatched\t{unmatched}\n")
            r.write(f"unique_donor_keys\t{len(lookup)}\n")
            for src, cnt in sorted(by_source.items(), key=lambda x: -x[1]):
                r.write(f"source_{src}\t{cnt}\n")


if __name__ == "__main__":
    main()
