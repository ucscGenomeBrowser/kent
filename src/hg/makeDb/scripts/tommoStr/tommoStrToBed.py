#!/usr/bin/env python3
"""
Convert ToMMo 61KJPN STR Expansion Hunter VCF to BED9+ for bigBed.

Input: VCF from ToMMo with <STRn> ALT alleles and AC/AF/MEAN/MEDIAN INFO fields.
Output: BED to stdout.

Usage:
    python3 tommoStrToBed.py input.vcf.gz | sort -k1,1 -k2,2n > tommoStr.bed
"""

import sys
import gzip
import re

HET_BINS = [
    (0.1, "0,0,180"),       # het < 0.1: dark blue (nearly monomorphic)
    (0.3, "70,130,230"),    # het 0.1-0.3: medium blue (low diversity)
    (0.5, "180,130,200"),   # het 0.3-0.5: light purple (moderate diversity)
    (0.7, "230,100,80"),    # het 0.5-0.7: salmon (high diversity)
    (999, "180,0,0"),       # het >= 0.7: dark red (very high diversity)
]
NO_DATA_COLOR = "128,128,128"  # gray when no allele freq data


def het_to_color(het):
    """Map heterozygosity value to an RGB color string."""
    if het < 0:
        return NO_DATA_COLOR
    for threshold, color in HET_BINS:
        if het < threshold:
            return color
    return HET_BINS[-1][1]


def compute_het(hist_parts):
    """Compute expected heterozygosity from allele count pairs.

    hist_parts: list of (copies, count) tuples.
    Returns het = 1 - sum(p_i^2), or -1 if no data.
    """
    if not hist_parts:
        return -1.0
    total = sum(c for _, c in hist_parts)
    if total == 0:
        return -1.0
    sum_p_sq = sum((c / total) ** 2 for _, c in hist_parts)
    return round(1.0 - sum_p_sq, 3)


def truncate_motif(motif, max_len=25):
    """Truncate long motifs with .. in the middle."""
    if len(motif) <= max_len:
        return motif
    half = (max_len - 2) // 2
    return motif[:half] + ".." + motif[-half:]


def parse_info(info_str):
    """Parse VCF INFO field into a dict."""
    d = {}
    for item in info_str.split(";"):
        if "=" in item:
            key, val = item.split("=", 1)
            d[key] = val
        else:
            d[item] = True
    return d


def parse_str_alleles(alt_str):
    """Parse <STR0>,<STR1>,... into list of repeat copy numbers."""
    copies = []
    for a in alt_str.split(","):
        m = re.match(r"<STR(\d+)>", a)
        if m:
            copies.append(int(m.group(1)))
        else:
            copies.append(None)
    return copies


def main():
    if len(sys.argv) < 2:
        print("Usage: tommoStrToBed.py input.vcf.gz", file=sys.stderr)
        sys.exit(1)

    vcf_path = sys.argv[1]
    opener = gzip.open if vcf_path.endswith(".gz") else open

    count = 0
    with opener(vcf_path, "rt") as f:
        for line in f:
            if line.startswith("#"):
                continue

            parts = line.rstrip("\n").split("\t")
            if len(parts) < 8:
                continue

            chrom, pos, _, ref_allele, alt, _, _, info_str = parts[:8]
            info = parse_info(info_str)

            # Parse coordinates
            start = int(pos) - 1  # VCF is 1-based
            end = int(info.get("END", pos))

            # Parse repeat info
            motif = info.get("RU", ".")
            period = len(motif) if motif != "." else 0
            ref_copies = int(info.get("REF", "0"))
            mean_val = info.get("MEAN", "0")
            median_val = info.get("MEDIAN", "0")
            an = info.get("AN", "0")

            # Parse allele counts
            alt_copies = parse_str_alleles(alt)
            ac_vals = info.get("AC", "").split(",") if "AC" in info else []

            # Build histogram: copies=count pairs
            # Include the reference allele count
            hist_parts = []
            total_alt_ac = 0
            for i, copies in enumerate(alt_copies):
                if copies is None or i >= len(ac_vals):
                    continue
                try:
                    ac = int(ac_vals[i])
                except ValueError:
                    continue
                if ac > 0:
                    hist_parts.append((copies, ac))
                    total_alt_ac += ac

            # Add reference allele count (AN - sum of AC)
            try:
                an_int = int(an)
                ref_ac = an_int - total_alt_ac
                if ref_ac > 0:
                    hist_parts.append((ref_copies, ref_ac))
            except ValueError:
                pass

            # Sort by copy number
            hist_parts.sort(key=lambda x: x[0])
            hist_str = " ".join(f"{c}={n}" for c, n in hist_parts)
            if not hist_str:
                hist_str = "."

            # Compute heterozygosity and color
            het = compute_het(hist_parts)
            color = het_to_color(het)
            score = max(0, int(het * 1000)) if het >= 0 else 0

            # Name
            motif_display = truncate_motif(motif)
            name = f"{motif_display} x{ref_copies}"

            fields = [
                chrom, str(start), str(end), name, str(score), ".",
                str(start), str(end), color,
                motif, str(period), str(ref_copies),
                mean_val, median_val, an, str(het), hist_str,
            ]
            print("\t".join(fields))
            count += 1

    print(f"Wrote {count:,} loci", file=sys.stderr)


if __name__ == "__main__":
    main()
