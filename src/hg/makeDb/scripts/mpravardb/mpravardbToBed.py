#!/usr/bin/env python3
"""
Convert mpravardb.csv to BED9+ format, liftOver hg19 to hg38,
merge, and create bigBed.

Usage:
    cd /hive/data/genomes/hg38/bed/mpra/mpravardb
    python3 ~/kent/src/hg/makeDb/scripts/mpravardb/mpravardbToBed.py
"""

import csv
import subprocess
import sys
import os
import math

SCRIPT_DIR = os.path.join(os.environ["HOME"], "kent/src/hg/makeDb/scripts/mpravardb")
AS_FILE = os.path.join(SCRIPT_DIR, "mpravardb.as")
LIFTOVER_CHAIN = "/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz"
CHROM_SIZES = "/hive/data/genomes/hg38/chrom.sizes"
INPUT_CSV = "mpravardb.csv"

def pval_to_score(pval):
    """Convert p-value to a 0-1000 score using -log10."""
    if pval is None or pval == "":
        return 0
    try:
        p = float(pval)
    except ValueError:
        return 0
    if p <= 0:
        return 1000
    score = int(-math.log10(p) * 100)
    return max(0, min(1000, score))

def pval_to_color(pval, fdr):
    """Color by significance: red if FDR<0.05, orange if p<0.05, black otherwise."""
    try:
        fdr_val = float(fdr) if fdr not in (None, "", "NA") else 1.0
    except ValueError:
        fdr_val = 1.0
    try:
        p_val = float(pval) if pval not in (None, "", "NA") else 1.0
    except ValueError:
        p_val = 1.0

    if fdr_val < 0.05:
        return "200,0,0"     # dark red - FDR significant
    elif p_val < 0.05:
        return "255,165,0"   # orange - nominally significant
    else:
        return "190,190,190" # grey - not significant

def safe_float(val):
    """Convert to float, return 0.0 for NA or empty."""
    if val in (None, "", "NA"):
        return 0.0
    try:
        return float(val)
    except ValueError:
        return 0.0

def csv_to_bed(input_csv, hg19_bed, hg38_bed):
    """Parse CSV and write two BED files, one per assembly."""
    hg19_count = 0
    hg38_count = 0
    with open(input_csv, newline="") as fin, \
         open(hg19_bed, "w") as f19, \
         open(hg38_bed, "w") as f38:
        reader = csv.reader(fin)
        header = next(reader)
        for row in reader:
            chrom_num, pos, ref, alt, genome = row[0], row[1], row[2], row[3], row[4]
            rsid, disease, cellline = row[5], row[6], row[7]
            desc, log2fc, pvalue, fdr, study = row[8], row[9], row[10], row[11], row[12]

            chrom = "chr" + chrom_num
            try:
                start = int(pos) - 1  # CSV uses 1-based coordinates
            except ValueError:
                continue
            end = start + max(1, len(ref))  # span the reference allele

            # Build name
            if rsid and rsid != "NA":
                name = rsid
            else:
                name = f"{chrom}:{pos}:{ref}>{alt}"

            score = pval_to_score(pvalue)
            color = pval_to_color(pvalue, fdr)

            # Truncate long string fields to stay within bigBed limits
            if len(desc) > 250:
                desc = desc[:247] + "..."
            if len(study) > 250:
                study = study[:247] + "..."

            # Short values for mouseOver (3 significant digits)
            log2fc_val = safe_float(log2fc)
            pvalue_val = safe_float(pvalue)
            fdr_val = safe_float(fdr)
            mo_log2fc = f"{log2fc_val:.3g}"
            mo_pvalue = f"{pvalue_val:.3g}"
            mo_fdr = f"{fdr_val:.3g}"

            fields = [
                chrom, str(start), str(end), name, str(score), ".",
                str(start), str(end), color,
                ref, alt,
                rsid if rsid and rsid != "NA" else ".",
                disease, cellline, desc,
                str(log2fc_val),
                str(pvalue_val),
                str(fdr_val),
                study,
                mo_log2fc, mo_pvalue, mo_fdr,
            ]
            line = "\t".join(fields) + "\n"

            if genome == "hg19":
                f19.write(line)
                hg19_count += 1
            elif genome == "hg38":
                f38.write(line)
                hg38_count += 1

    print(f"Wrote {hg19_count} hg19 rows to {hg19_bed}")
    print(f"Wrote {hg38_count} hg38 rows to {hg38_bed}")

def run(cmd):
    """Run a shell command, exit on failure."""
    print(f"  Running: {cmd}")
    result = subprocess.run(cmd, shell=True)
    if result.returncode != 0:
        print(f"ERROR: command failed with exit code {result.returncode}", file=sys.stderr)
        sys.exit(1)

def main():
    hg19_bed = "mpravardb.hg19.bed"
    hg38_bed = "mpravardb.hg38.bed"
    lifted_bed = "mpravardb.hg19lifted.bed"
    unmapped_bed = "mpravardb.unmapped.bed"
    merged_bed = "mpravardb.bed"
    output_bb = "mpravardb.bb"

    print("Step 1: Converting CSV to BED format...")
    csv_to_bed(INPUT_CSV, hg19_bed, hg38_bed)

    print("\nStep 2: Lifting hg19 coordinates to hg38...")
    run(f"liftOver -bedPlus=9 -tab {hg19_bed} {LIFTOVER_CHAIN} {lifted_bed} {unmapped_bed}")

    # Count unmapped
    unmapped = sum(1 for line in open(unmapped_bed) if not line.startswith("#"))
    lifted = sum(1 for _ in open(lifted_bed))
    print(f"  Lifted: {lifted}, Unmapped: {unmapped}")

    print("\nStep 3: Merging hg38 native + lifted hg19...")
    run(f"cat {hg38_bed} {lifted_bed} > {merged_bed}.tmp")
    run(f"bedSort {merged_bed}.tmp {merged_bed}")
    os.remove(f"{merged_bed}.tmp")

    total = sum(1 for _ in open(merged_bed))
    print(f"  Total merged rows: {total}")

    print("\nStep 4: Converting to bigBed...")
    run(f"bedToBigBed -type=bed9+ -tab -as={AS_FILE} {merged_bed} {CHROM_SIZES} {output_bb}")

    print(f"\nDone. Output: {output_bb}")
    print(f"File size: {os.path.getsize(output_bb) / 1024 / 1024:.1f} MB")

if __name__ == "__main__":
    main()
