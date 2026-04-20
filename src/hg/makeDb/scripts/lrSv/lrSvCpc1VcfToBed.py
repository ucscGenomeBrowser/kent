#!/usr/bin/env python3
"""Convert the CPC+HPRC Phase 1 pangenome VCF (on T2T-CHM13v2) to a
bed9+7 table for the lrSv/cpc1Sv track.

The input VCF is produced by vcfwave/bcftools norm from a pangenome
graph, so:
  * contigs are named "CHM13v2.chrN"
  * variant IDs are graph snarl traversals like ">2541>2547"
  * REF and ALT are always explicit sequences (no symbolic ALTs)
  * each snarl may appear as several rows, one per decomposed alt allele

This script:
  1. strips the "CHM13v2." contig prefix (hs1 chrom names)
  2. classifies each alt allele by length delta d = len(ALT)-len(REF):
         d >= +50   -> INS
         d <= -50   -> DEL
         |d| < 50 and max(len) >= 50 -> CPX
         otherwise  -> dropped (below 50bp threshold)
  3. groups all alt alleles that share the same graph snarl ID into one
     output row, taking:
         svType = that class if all alts agree, else MIXED
         svLen  = max |d| across alts
         alleleCount = sum of per-alt AC
         alleleNumber = AN (constant for the snarl)
         alleleFreq   = alleleCount / alleleNumber
     The BED interval spans from POS-1 to POS-1 + len(REF).

Usage:
    lrSvCpc1VcfToBed.py input.vcf.gz output.bed chrom.sizes
"""

import gzip
import sys
from collections import defaultdict

# Colors per SV type
COLORS = {
    "INS": "0,0,200",       # blue
    "DEL": "200,0,0",       # red
    "CPX": "230,140,0",     # orange
    "MIXED": "120,120,120", # grey
}

SIZE_THRESHOLD = 50


def parse_info(info_str):
    d = {}
    for token in info_str.split(";"):
        if not token:
            continue
        if "=" in token:
            k, v = token.split("=", 1)
            d[k] = v
        else:
            d[token] = True
    return d


def classify(ref_len, alt_len):
    d = alt_len - ref_len
    if d >= SIZE_THRESHOLD:
        return "INS", d
    if d <= -SIZE_THRESHOLD:
        return "DEL", -d
    if max(ref_len, alt_len) >= SIZE_THRESHOLD:
        return "CPX", max(ref_len, alt_len)
    return None, 0


def emit(site, fout):
    """site is dict: chrom, pos0, ref_len, name, types (set), svlen,
    ac_sum, an, ns."""
    classes = site["types"]
    if len(classes) == 1:
        sv_type = next(iter(classes))
    else:
        sv_type = "MIXED"
    rgb = COLORS[sv_type]
    chrom = site["chrom"]
    start = site["pos0"]
    end = start + max(site["ref_len"], 1)
    af = (site["ac_sum"] / site["an"]) if site["an"] else 0.0
    score = min(1000, max(0, int(round(af * 1000))))
    row = [
        chrom, str(start), str(end),
        site["name"],
        str(score),
        ".",
        str(start), str(end),
        rgb,
        sv_type,
        str(site["svlen"]),
        str(site["num_alts"]),
        str(site["ac_sum"]),
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

    # Load chrom sizes for validation / clipping
    chrom_sizes = {}
    with open(sizes_file) as f:
        for line in f:
            c, s = line.strip().split("\t")
            chrom_sizes[c] = int(s)

    opener = gzip.open if in_file.endswith(".gz") else open

    # Accumulator: key = (chrom, pos0, snarl_id)
    # We stream the file and flush a site whenever we encounter a new key,
    # because the VCF is sorted and decomposed alts of the same snarl are
    # adjacent.
    prev_key = None
    site = None
    kept_rows = 0
    dropped_small = 0
    dropped_chrom = 0
    flushed_sites = 0

    with opener(in_file, "rt") as fin, open(out_file, "w") as fout:
        for line in fin:
            if line.startswith("#"):
                continue
            f = line.rstrip("\n").split("\t", 8)
            chrom_raw, pos, vid, ref, alt, qual, filt, info = f[:8]
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

            info_d = parse_info(info)
            def _int(val, default=0):
                try:
                    return int(str(val).split(",")[0])
                except (ValueError, TypeError):
                    return default
            ac = _int(info_d.get("AC", 0))
            an = _int(info_d.get("AN", 0))
            ns = _int(info_d.get("NS", 0))
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
                    "num_alts": 0,
                    "ac_sum": 0,
                    "an": an,
                    "ns": ns,
                }
                prev_key = key

            site["types"].add(sv_type)
            if mag > site["svlen"]:
                site["svlen"] = mag
            site["num_alts"] += 1
            site["ac_sum"] += ac
            # AN, NS should be the same across alts of one site; keep max just in case
            if an > site["an"]:
                site["an"] = an
            if ns > site["ns"]:
                site["ns"] = ns
            kept_rows += 1

        if site is not None:
            emit(site, fout)
            flushed_sites += 1

    print(f"kept alt rows:        {kept_rows}", file=sys.stderr)
    print(f"dropped <{SIZE_THRESHOLD}bp alt rows: {dropped_small}", file=sys.stderr)
    print(f"dropped bad chrom:    {dropped_chrom}", file=sys.stderr)
    print(f"output sites:         {flushed_sites}", file=sys.stderr)


if __name__ == "__main__":
    main()
