#!/usr/bin/env python3
"""
Convert mpravardb.csv to BED9+ format, liftOver hg19 to hg38,
merge, and create bigBed.

Usage:
    cd /hive/data/genomes/hg38/bed/mpra/mpravardb
    python3 ~/kent/src/hg/makeDb/scripts/mpravardb/mpravardbToBed.py
"""

import csv
import re
import subprocess
import sys
import os
import math

SCRIPT_DIR = os.path.join(os.environ["HOME"], "kent/src/hg/makeDb/scripts/mpravardb")
AS_FILE = os.path.join(SCRIPT_DIR, "mpravardb.as")
LIFTOVER_CHAIN = "/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz"
CHROM_SIZES = "/hive/data/genomes/hg38/chrom.sizes"
INPUT_CSV = "mpravardb.csv"

# Upstream CSV contains UTF-8 curly quotes, primes, and NBSP mojibake.
# Browser does not transcode UTF-8 in bigBed fields, so everything user-visible
# must be plain ASCII. Transliterate or strip. Keys use \u escapes rather than
# literal characters so an editor can't silently re-encode them.
_SANITIZE_MAP = str.maketrans({
    "\u2019": "'",   # RIGHT SINGLE QUOTATION MARK (used as apostrophe)
    "\u2018": "'",   # LEFT SINGLE QUOTATION MARK
    "\u201c": '"',   # LEFT DOUBLE QUOTATION MARK
    "\u201d": '"',   # RIGHT DOUBLE QUOTATION MARK
    "\u2032": "'",   # PRIME (used after numerals: 3'UTR)
    "\u2033": '"',   # DOUBLE PRIME
    "\u2013": "-",   # EN DASH
    "\u2014": "-",   # EM DASH
    "\u2026": "...", # HORIZONTAL ELLIPSIS
    "\u00a0": " ",   # NO-BREAK SPACE
    "\u00ac": "",    # NOT SIGN  (NBSP mojibake pair)
    "\u2020": "",    # DAGGER    (NBSP mojibake pair)
})
_WS_RE = re.compile(r"\s+")

# Literal typos in the upstream MPRAVarDB CSV that are visible in user-facing
# fields (description, disease).  Repaired here on every build until upstream
# curators fix them.  Counts at first observation 2026-05-01:
#   - "30 UTR" : 26,546 Schuster 2023 description rows (should be "3'UTR")
#   - "Familial hypercholesterol emia" : 2,176 Kircher 2019 disease rows
#   - "Alchol use disorder" : 88 Rao 2021 disease rows
_TYPO_FIXES = {
    "30 UTR": "3'UTR",
    "Familial hypercholesterol emia": "Familial hypercholesterolemia",
    "Alchol use disorder": "Alcohol use disorder",
}
_TYPO_FIX_RE = re.compile("|".join(re.escape(k) for k in _TYPO_FIXES))

# Sentinel strings that upstream uses to mean "no value".  Standardize to ""
# so mouseOver and details don't carry "None" / "NA" / "nan" tokens.
_NA_SENTINELS = {"None", "NA", "N/A", "null", "NULL", "nan"}

def sanitize_text(s):
    """Return ASCII-only, sentinel-normalized version of s for bigBed string fields."""
    if not s:
        return ""
    s = s.translate(_SANITIZE_MAP)
    s = _TYPO_FIX_RE.sub(lambda m: _TYPO_FIXES[m.group()], s)
    # Drop any remaining non-ASCII (rare), then collapse runs of whitespace
    s = s.encode("ascii", "ignore").decode("ascii")
    s = _WS_RE.sub(" ", s).strip()
    if s in _NA_SENTINELS:
        return ""
    return s

def fmt_mo(val):
    """Format a float for mouseOver helper fields.  Renders NaN as 'NA'
    (rather than literal 'nan') so the mouseOver reads 'p-value: NA' on
    untested variants."""
    if math.isnan(val):
        return "NA"
    return f"{val:.3g}"

def pval_to_score(pval):
    """Convert p-value to a 0-1000 score using -log10.
    Missing / out-of-range / non-numeric → 0 (not 1000).
    Many rows upstream encode NA as literal 0.0, which is indistinguishable from
    a true p=0; treat all non-positive inputs as unscored."""
    if pval is None or pval in ("", "NA"):
        return 0
    try:
        p = float(pval)
    except ValueError:
        return 0
    if p <= 0 or p > 1:
        return 0
    score = int(-math.log10(p) * 100)
    return max(0, min(1000, score))

def pval_to_color(pval, fdr):
    """Color by significance: red if FDR<0.05, orange if p<0.05, grey otherwise."""
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
    """Convert to float, return NaN for NA / empty / non-numeric.
    Using NaN (rather than 0.0) keeps untested variants out of filterByRange
    sliders by default and avoids masquerading as p=0 / fdr=0 in the details
    page.  bedToBigBed accepts the literal string "nan" in float fields."""
    if val in (None, "", "NA"):
        return math.nan
    try:
        return float(val)
    except ValueError:
        return math.nan

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

            # Sanitize user-visible string fields (ASCII only, drop NBSP mojibake)
            rsid = sanitize_text(rsid)
            disease = sanitize_text(disease)
            cellline = sanitize_text(cellline)
            desc = sanitize_text(desc)
            study = sanitize_text(study)
            ref = sanitize_text(ref)
            alt = sanitize_text(alt)

            chrom = "chr" + chrom_num
            try:
                start = int(pos) - 1  # CSV uses 1-based coordinates
            except ValueError:
                continue
            end = start + max(1, len(ref))  # span the reference allele

            # Treat rsid as authoritative only if it actually looks like one.
            # Upstream preserves hg19-coord-style strings (e.g. "1_1403972_CG")
            # in the rsid column for ~2,088 rows; those would otherwise leak
            # into name + rsid field + dbSNP linkout.
            if rsid.startswith("rs"):
                name = rsid
                rsid_field = rsid
            else:
                name = f"{chrom}:{pos}:{ref}>{alt}"
                rsid_field = ""

            score = pval_to_score(pvalue)
            color = pval_to_color(pvalue, fdr)

            log2fc_val = safe_float(log2fc)
            pvalue_val = safe_float(pvalue)
            fdr_val = safe_float(fdr)

            fields = [
                chrom, str(start), str(end), name, str(score), ".",
                str(start), str(end), color,
                ref, alt,
                rsid_field,
                disease, cellline, desc,
                str(log2fc_val),
                str(pvalue_val),
                str(fdr_val),
                study,
                fmt_mo(log2fc_val), fmt_mo(pvalue_val), fmt_mo(fdr_val),
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

# Non-rs names are built in csv_to_bed() as "chr<X>:<pos>:<ref>><alt>"
# from the raw CSV pos, which for hg19 rows is the hg19 position. liftOver
# updates the BED coordinates but leaves the name string alone, so the name
# carries a stale hg19 pos. Rewrite the chrom:pos prefix from the row's
# (already hg38-coordinate) col 1 + col 2 so the name agrees with the row's
# coordinates.
_NAME_PREFIX_RE = re.compile(r"^chr[^:]+:\d+:")

def fix_lifted_names(lifted_bed):
    fixed = 0
    tmp = lifted_bed + ".tmp"
    with open(lifted_bed) as fin, open(tmp, "w") as fout:
        for line in fin:
            cols = line.rstrip("\n").split("\t")
            chrom, start, name = cols[0], int(cols[1]), cols[3]
            if _NAME_PREFIX_RE.match(name):
                new_name = _NAME_PREFIX_RE.sub(f"{chrom}:{start+1}:", name)
                if new_name != name:
                    cols[3] = new_name
                    fixed += 1
            fout.write("\t".join(cols) + "\n")
    os.replace(tmp, lifted_bed)
    print(f"  Rewrote hg19 -> hg38 pos in {fixed} non-rs name fields")

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

    # Rewrite chr:pos inside non-rs name fields with the lifted hg38 pos.
    fix_lifted_names(lifted_bed)

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
