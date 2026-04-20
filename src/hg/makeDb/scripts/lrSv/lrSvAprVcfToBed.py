#!/usr/bin/env python3
"""Convert Arabic Pangenome Reference (APR) pangenome VCF on T2T-CHM13v2
to a bed9+7 table for the lrSv/aprSv track.

Input VCF characteristics (unlike the CPC VCF this one is NOT decomposed):
  * contigs are named "chr1", "chr2", ... with CHM13v2 lengths
  * variant IDs are graph snarl IDs like "<951452<1012008"
  * REF/ALT are explicit sequences
  * each snarl appears on a single row, with multi-allelic ALT
    (comma-separated). INFO AC/AF are comma lists of the same length.

This script:
  1. iterates the comma-separated alt alleles within each VCF row
  2. classifies each alt by length delta d = len(ALT)-len(REF):
         d >= +50   -> INS
         d <= -50   -> DEL
         |d| < 50 and max(len) >= 50 -> CPX
         otherwise  -> dropped
  3. emits one bed row per snarl (row) with alts merged:
         svType = agreed class or MIXED if alts differ
         svLen  = max |d| across passing alts
         alleleCount = sum of AC values for passing alts
         alleleNumber = AN (constant)
         alleleFreq   = alleleCount / alleleNumber
     Rows with zero passing alts are skipped.

Usage:
    lrSvAprVcfToBed.py input.vcf.gz output.bed chrom.sizes
"""

import gzip
import sys

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


def _int(val, default=0):
    try:
        return int(str(val))
    except (ValueError, TypeError):
        return default


def main():
    if len(sys.argv) < 4:
        sys.exit("usage: lrSvAprVcfToBed.py input.vcf.gz output.bed chrom.sizes")
    in_file = sys.argv[1]
    out_file = sys.argv[2]
    sizes_file = sys.argv[3]

    chrom_sizes = {}
    with open(sizes_file) as f:
        for line in f:
            c, s = line.strip().split("\t")
            chrom_sizes[c] = int(s)

    opener = gzip.open if in_file.endswith(".gz") else open

    emitted = 0
    skipped_no_sv_alt = 0
    skipped_chrom = 0
    total_alts_seen = 0

    with opener(in_file, "rt") as fin, open(out_file, "w") as fout:
        for line in fin:
            if line.startswith("#"):
                continue
            f = line.rstrip("\n").split("\t", 8)
            chrom, pos, vid, ref, alt, qual, filt, info = f[:8]

            if chrom not in chrom_sizes:
                skipped_chrom += 1
                continue

            alts = alt.split(",")
            total_alts_seen += len(alts)
            info_d = parse_info(info)

            ac_list = info_d.get("AC", "").split(",")
            af_list = info_d.get("AF", "").split(",")
            an = _int(info_d.get("AN", "0"))
            ns = _int(info_d.get("NS", "0"))
            ref_len = len(ref)

            types = set()
            max_mag = 0
            ac_sum = 0
            num_pass = 0
            for i, alt_seq in enumerate(alts):
                sv_type, mag = classify(ref_len, len(alt_seq))
                if sv_type is None:
                    continue
                types.add(sv_type)
                if mag > max_mag:
                    max_mag = mag
                if i < len(ac_list):
                    ac_sum += _int(ac_list[i])
                num_pass += 1

            if num_pass == 0:
                skipped_no_sv_alt += 1
                continue

            sv_type = next(iter(types)) if len(types) == 1 else "MIXED"
            rgb = COLORS[sv_type]

            pos0 = int(pos) - 1
            start = pos0
            end = start + max(ref_len, 1)
            af = (ac_sum / an) if an else 0.0
            score = min(1000, max(0, int(round(af * 1000))))

            row = [
                chrom, str(start), str(end),
                vid,
                str(score),
                ".",
                str(start), str(end),
                rgb,
                sv_type,
                str(max_mag),
                str(num_pass),
                str(ac_sum),
                str(an),
                f"{af:.6f}",
                str(ns),
            ]
            fout.write("\t".join(row) + "\n")
            emitted += 1

    print(f"total alt alleles seen: {total_alts_seen}", file=sys.stderr)
    print(f"emitted sites (>=1 SV alt): {emitted}", file=sys.stderr)
    print(f"skipped rows (no SV alt): {skipped_no_sv_alt}", file=sys.stderr)
    print(f"skipped rows (bad chrom): {skipped_chrom}", file=sys.stderr)


if __name__ == "__main__":
    main()
