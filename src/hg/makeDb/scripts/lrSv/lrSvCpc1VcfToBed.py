#!/usr/bin/env python3
"""Convert the CPC+HPRC Phase 1 pangenome VCF (on T2T-CHM13v2) to a
bed9+7 table for the lrSv/cpc1Sv track, RECOMPUTING AC/AN/NS using only
the 58 CPC (Chinese Pangenome Consortium) sample columns and dropping
snarls with no CPC carriers.

The input VCF is produced by vcfwave/bcftools norm from a pangenome
graph, so:
  * contigs are named "CHM13v2.chrN"
  * variant IDs are graph snarl traversals like ">2541>2547"
  * REF and ALT are always explicit sequences (no symbolic ALTs)
  * each snarl may appear as several rows, one per decomposed alt allele
  * the 105 sample columns are 47 HPRC (HG*, NA*) + 58 CPC (HIFI032*, RY*)

Pipeline:
  1. Parse the #CHROM header to identify the 58 CPC sample column indices
     by prefix (HIFI032* or RY*).
  2. Strip the "CHM13v2." contig prefix for hs1 chrom names.
  3. For each VCF row, read the GT field for each CPC sample and compute
     CPC-only AC (count of "1" alleles), AN (count of non-missing alleles),
     and NS (CPC samples with at least one called allele).
  4. Classify each alt allele by length delta d = len(ALT)-len(REF):
         d >= +50   -> INS
         d <= -50   -> DEL
         |d| < 50 and max(len) >= 50 -> CPX
         otherwise  -> dropped (below 50bp threshold)
  5. Drop rows where CPC AC == 0 (HPRC-specific alts).
  6. Collapse all remaining alts sharing the same snarl ID into one output row:
         svType = that class if all alts agree, else MIXED
         svLen  = reference span (chromEnd - chromStart)
         insLen = max inserted-sequence length for INS alts (0 otherwise)
         AC     = sum of per-alt CPC AC
         alleleNumber = CPC AN
         alleleFreq   = AC / alleleNumber
         numSamples   = CPC NS

Usage:
    lrSvCpc1VcfToBed.py input.vcf.gz output.bed chrom.sizes
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType, svColor

SIZE_THRESHOLD = 50


def is_cpc_sample(name):
    """CPC samples are HIFI032* (Chinese, 47) and RY* (Chinese, 11)."""
    return name.startswith("HIFI032") or name.startswith("RY")


def classify(ref_len, alt_len):
    d = alt_len - ref_len
    if d >= SIZE_THRESHOLD:
        return "INS", d
    if d <= -SIZE_THRESHOLD:
        return "DEL", -d
    if max(ref_len, alt_len) >= SIZE_THRESHOLD:
        return "CPX", max(ref_len, alt_len)
    return None, 0


def compute_cpc_counts(gt_cols):
    """Return (ac, an, ns) across the given CPC GT strings for a single alt."""
    ac = 0
    an = 0
    ns = 0
    for gt in gt_cols:
        # GT may be "0|0", "1|0", ".|1", ".|.", "0/1", etc.
        has_called = False
        for a in gt.replace("/", "|").split("|"):
            if a == ".":
                continue
            an += 1
            has_called = True
            if a == "1":
                ac += 1
        if has_called:
            ns += 1
    return ac, an, ns


def emit(site, fout):
    classes = site["types"]
    sv_type = next(iter(classes)) if len(classes) == 1 else "MIXED"
    sv_type = normalizeSvType(sv_type)
    rgb = svColor(sv_type)
    chrom = site["chrom"]
    start = site["pos0"]
    end = start + max(site["ref_len"], 1)
    af = (site["ac_sum"] / site["an"]) if site["an"] else 0.0
    score = min(1000, max(0, int(round(af * 1000))))
    svLen = end - start
    insLen = site["max_ins"] if sv_type == "INS" else 0
    featLen = insLen if sv_type in ("INS", "MEI") else svLen
    name = svName(sv_type, featLen, site["ac_sum"])
    row = [
        chrom, str(start), str(end),
        name,
        str(score),
        ".",
        str(start), str(end),
        rgb,
        sv_type,
        str(svLen),
        str(insLen),
        str(site["ac_sum"]),
        str(site["num_alts"]),
        str(site["an"]),
        f"{af:.6f}",
        str(site["ns"]),
    ]
    fout.write("\t".join(row) + "\n")


def main():
    if len(sys.argv) < 4:
        sys.exit("usage: lrSvCpc1VcfToBed.py input.vcf.gz output.bed chrom.sizes")
    in_file = sys.argv[1]
    out_file = sys.argv[2]
    sizes_file = sys.argv[3]

    chrom_sizes = {}
    with open(sizes_file) as f:
        for line in f:
            c, s = line.strip().split("\t")
            chrom_sizes[c] = int(s)

    opener = gzip.open if in_file.endswith(".gz") else open

    cpc_col_idx = None  # set when we parse #CHROM header

    prev_key = None
    site = None
    kept_rows = 0
    dropped_small = 0
    dropped_no_cpc_carrier = 0
    dropped_chrom = 0
    flushed_sites = 0

    with opener(in_file, "rt") as fin, open(out_file, "w") as fout:
        for line in fin:
            if line.startswith("##"):
                continue
            if line.startswith("#CHROM"):
                # Sample columns start at index 9.
                cols = line.rstrip("\n").split("\t")
                sample_names = cols[9:]
                cpc_col_idx = [
                    9 + i for i, n in enumerate(sample_names) if is_cpc_sample(n)
                ]
                print(f"CPC samples found: {len(cpc_col_idx)}", file=sys.stderr)
                if len(cpc_col_idx) == 0:
                    sys.exit("ERROR: no CPC sample columns (HIFI032* or RY*) detected")
                continue

            if cpc_col_idx is None:
                sys.exit("ERROR: data line before #CHROM header")

            f = line.rstrip("\n").split("\t")
            chrom_raw, pos, vid, ref, alt = f[0], f[1], f[2], f[3], f[4]

            if chrom_raw.startswith("CHM13v2."):
                chrom = chrom_raw[len("CHM13v2."):]
            else:
                chrom = chrom_raw
            if chrom not in chrom_sizes:
                dropped_chrom += 1
                continue

            ref_len = len(ref)
            alt_len = len(alt)
            sv_type, mag = classify(ref_len, alt_len)
            if sv_type is None:
                dropped_small += 1
                continue

            # Extract CPC GTs and compute AC/AN/NS
            gt_cols = [f[i] for i in cpc_col_idx]
            ac, an, ns = compute_cpc_counts(gt_cols)
            if ac == 0:
                dropped_no_cpc_carrier += 1
                continue

            pos0 = int(pos) - 1
            key = (chrom, pos0, vid)
            if key != prev_key:
                if site is not None:
                    emit(site, fout)
                    flushed_sites += 1
                site = {
                    "chrom": chrom,
                    "pos0": pos0,
                    "ref_len": ref_len,
                    "name": vid,
                    "types": set(),
                    "svlen": 0,
                    "max_ins": 0,
                    "num_alts": 0,
                    "ac_sum": 0,
                    "an": an,
                    "ns": ns,
                }
                prev_key = key

            site["types"].add(sv_type)
            if mag > site["svlen"]:
                site["svlen"] = mag
            if sv_type == "INS":
                d = alt_len - ref_len
                if d > site["max_ins"]:
                    site["max_ins"] = d
            site["num_alts"] += 1
            site["ac_sum"] += ac
            if an > site["an"]:
                site["an"] = an
            if ns > site["ns"]:
                site["ns"] = ns
            kept_rows += 1

        if site is not None:
            emit(site, fout)
            flushed_sites += 1

    print(f"kept alt rows (CPC carrier):   {kept_rows}", file=sys.stderr)
    print(f"dropped no CPC carrier:        {dropped_no_cpc_carrier}", file=sys.stderr)
    print(f"dropped <{SIZE_THRESHOLD}bp alt rows: {dropped_small}", file=sys.stderr)
    print(f"dropped bad chrom:             {dropped_chrom}", file=sys.stderr)
    print(f"output sites:                  {flushed_sites}", file=sys.stderr)


if __name__ == "__main__":
    main()
