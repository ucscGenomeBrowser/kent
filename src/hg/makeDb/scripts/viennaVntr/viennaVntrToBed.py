#!/usr/bin/env python3
"""
Convert Vienna ONT 1000 Genomes VAMOS VNTR summary TSV to BED9+ for bigBed.

Input: vamos-summary.tsv from the Vienna ONT project.
Output: BED to stdout.

Usage:
    python3 viennaVntrToBed.py vamos-summary.tsv | sort -k1,1 -k2,2n > viennaVntr.bed
"""

import sys

HET_BINS = [
    (0.1, "0,0,180"),       # het < 0.1: dark blue (nearly monomorphic)
    (0.3, "70,130,230"),    # het 0.1-0.3: medium blue (low diversity)
    (0.5, "180,130,200"),   # het 0.3-0.5: light purple (moderate diversity)
    (0.7, "230,100,80"),    # het 0.5-0.7: salmon (high diversity)
    (999, "180,0,0"),       # het >= 0.7: dark red (very high diversity)
]
NO_DATA_COLOR = "128,128,128"  # gray when no het data


def het_to_color(het):
    """Map heterozygosity value to an RGB color string."""
    if het < 0:
        return NO_DATA_COLOR
    for threshold, color in HET_BINS:
        if het < threshold:
            return color
    return HET_BINS[-1][1]


def load_het(het_path):
    """Load het values from TSV (chrom, pos_1based, het)."""
    het_map = {}
    with open(het_path) as f:
        for line in f:
            chrom, pos, het = line.rstrip("\n").split("\t")
            het_map[(chrom, int(pos))] = float(het)
    return het_map


def main():
    if len(sys.argv) < 3:
        print("Usage: viennaVntrToBed.py vamos-summary.tsv het.tsv", file=sys.stderr)
        sys.exit(1)

    print("Loading het values...", file=sys.stderr)
    het_map = load_het(sys.argv[2])
    print(f"  Loaded {len(het_map)} het values", file=sys.stderr)

    count = 0
    with open(sys.argv[1]) as f:
        header = None
        for line in f:
            if line.startswith("#"):
                header = line.strip().lstrip("#").split("\t")
                continue

            parts = line.strip().split("\t")
            if len(parts) < 20:
                continue

            def safe_int(s, default=0):
                try:
                    v = float(s)
                    if v != v:  # NaN check
                        return default
                    return int(v)
                except (ValueError, OverflowError):
                    return default

            def safe_float(s, default="0"):
                try:
                    v = float(s)
                    if v != v:  # NaN
                        return default
                    return s
                except ValueError:
                    return default

            chrom = parts[0]
            start = int(parts[1]) - 1  # 1-based to 0-based
            end = int(parts[2])
            num_haps = safe_int(parts[3])
            ru_len_avg = float(parts[4]) if parts[4] != "NaN" else 0.0
            num_unique = safe_int(parts[6])
            max_rus = safe_int(parts[7])
            min_rus = safe_int(parts[8])
            median_rus = safe_float(parts[9])
            pct25_rus = safe_float(parts[12])
            pct75_rus = safe_float(parts[13])
            unique_num_rus = safe_int(parts[16])
            max_bps = safe_int(parts[17])
            min_bps = safe_int(parts[18])
            median_bps = safe_float(parts[19])

            # Lookup het (key is chrom, 1-based pos from VCF)
            het = het_map.get((chrom, start + 1), -1.0)
            color = het_to_color(het)
            score = max(0, int(het * 1000)) if het >= 0 else 0

            # Name: motif size and median repeat count
            name = f"{int(round(ru_len_avg))}bp x{median_rus}"

            fields = [
                chrom, str(start), str(end), name, str(score), ".",
                str(start), str(end), color,
                str(num_haps),
                f"{ru_len_avg:.1f}",
                str(num_unique),
                str(max_rus), str(min_rus), median_rus,
                pct25_rus, pct75_rus,
                str(unique_num_rus),
                str(max_bps), str(min_bps), median_bps,
                str(het),
            ]
            print("\t".join(fields))
            count += 1

    print(f"Wrote {count:,} loci", file=sys.stderr)


if __name__ == "__main__":
    main()
