#!/usr/bin/env python3
"""
Extract expected heterozygosity from Vienna ONT VAMOS multi-sample VCF.

For each locus, counts allele occurrences from GT fields across all samples,
then computes het = 1 - sum(p_i^2).

Output: TSV with columns chrom, pos (1-based, matching VCF), het

Usage:
    python3 viennaVntrHet.py vamos-multisample.vcf > het.tsv
"""

import sys
from collections import Counter


def main():
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    vcf_path = sys.argv[1]
    count = 0

    with open(vcf_path) as f:
        for line in f:
            if line.startswith("#"):
                continue

            parts = line.split("\t", 10)  # only need first fields + genotypes block
            chrom = parts[0]
            pos = parts[1]

            # Parse all GT fields (field 9 onward)
            gt_block = line.rstrip("\n").split("\t")[9:]
            allele_counts = Counter()
            for gt in gt_block:
                if gt == "./." or gt == ".|.":
                    continue
                sep = "|" if "|" in gt else "/"
                for a in gt.split(sep):
                    if a != ".":
                        allele_counts[a] += 1

            total = sum(allele_counts.values())
            if total == 0:
                het = -1.0
            else:
                sum_p_sq = sum((c / total) ** 2 for c in allele_counts.values())
                het = round(1.0 - sum_p_sq, 3)

            print(f"{chrom}\t{pos}\t{het}")
            count += 1
            if count % 100000 == 0:
                print(f"  Processed {count} loci...", file=sys.stderr)

    print(f"Done. Wrote {count} loci.", file=sys.stderr)


if __name__ == "__main__":
    main()
