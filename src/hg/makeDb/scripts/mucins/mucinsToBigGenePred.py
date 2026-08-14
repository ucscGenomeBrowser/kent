#!/usr/bin/env python3
"""Convert the locus-specific mucin exon table to two bigGenePred bed files,
one for VNTR exons and one for non-VNTR exons.

Input columns (tab-separated, CRLF line endings, one header row):
    gene  transcript  exon  chr  start  stop  size  strand  VNTR_status

Each exon becomes a single-block bigGenePred entry. Coordinates in the
source file are 0-based half-open.
"""
import argparse
import sys
from pathlib import Path

VNTR_COLOR = "213,94,0"        # Okabe-Ito vermillion
NON_VNTR_COLOR = "0,114,178"   # Okabe-Ito blue


def convert(in_path: Path, out_vntr: Path, out_non_vntr: Path,
            chrom_sizes: Path) -> None:
    sizes = {}
    with open(chrom_sizes) as fh:
        for line in fh:
            parts = line.rstrip().split("\t")
            if len(parts) >= 2:
                sizes[parts[0]] = int(parts[1])

    n_vntr = 0
    n_non_vntr = 0
    skipped = 0
    with open(in_path, newline="") as fh, \
            open(out_vntr, "w") as fv, \
            open(out_non_vntr, "w") as fn:
        header = fh.readline()  # discard
        for raw in fh:
            row = raw.rstrip("\r\n").split("\t")
            if len(row) < 9:
                skipped += 1
                continue
            gene, transcript, exon, chrom, start, stop, size, strand, status = row[:9]
            start_i = int(start)
            stop_i = int(stop)
            chrom_size = sizes.get(chrom)
            if chrom_size is None or stop_i > chrom_size:
                sys.stderr.write(
                    f"WARN: {gene} {transcript} {exon} {chrom}:{start_i}-{stop_i} "
                    f"exceeds chrom size {chrom_size}; skipping\n")
                skipped += 1
                continue
            if stop_i - start_i != int(size):
                sys.stderr.write(
                    f"WARN: size mismatch {gene} {exon}: "
                    f"{stop_i-start_i} vs {size}\n")

            is_vntr = status == "VNTR_exon"
            color = VNTR_COLOR if is_vntr else NON_VNTR_COLOR
            length = stop_i - start_i
            name = f"{gene}_{exon}"
            fields = [
                chrom,
                str(start_i),
                str(stop_i),
                name,
                "0",            # score
                strand,
                str(start_i),   # thickStart
                str(stop_i),    # thickEnd
                color,
                "1",            # blockCount
                f"{length},",  # blockSizes
                "0,",          # chromStarts
                gene,           # name2
                "none",         # cdsStartStat
                "none",         # cdsEndStat
                "-1,",         # exonFrames
                status,         # type
                transcript,     # geneName
                gene,           # geneName2
                "mucin",        # geneType
            ]
            out_line = "\t".join(fields) + "\n"
            if is_vntr:
                fv.write(out_line)
                n_vntr += 1
            else:
                fn.write(out_line)
                n_non_vntr += 1

    sys.stderr.write(
        f"Wrote {n_vntr} VNTR exons and {n_non_vntr} non-VNTR exons "
        f"({skipped} skipped)\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input")
    ap.add_argument("--out-vntr", required=True)
    ap.add_argument("--out-non-vntr", required=True)
    ap.add_argument("--chrom-sizes", required=True)
    args = ap.parse_args()
    convert(Path(args.input), Path(args.out_vntr),
            Path(args.out_non_vntr), Path(args.chrom_sizes))


if __name__ == "__main__":
    main()
