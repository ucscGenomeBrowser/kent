#!/usr/bin/env python3
"""Convert a CoLoRSdb pbsv+Jasmine SV VCF (GRCh38 or T2T-CHM13) to the
bed9+15 representation used by the lrSv/colorsDbSv track.

The source VCF (pbsv 2.9.0 -> Jasmine merge, site-only) carries these
INFO fields: SVTYPE, END, SVLEN, AC, AN, NS, AC_Hom, AC_Het, AC_Hemi,
AF, HWE, ExcHet, nhomalt. All are preserved. insLen is derived as
max(0, len(ALT) - len(REF)) for INS and 0 for everything else so the
shared lrSv supertrack filter works.

Usage:
    lrSvColorsDbSvVcfToBed.py input.vcf.gz output.bed chrom.sizes
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType, svColor

# Only these SV types are kept from the CoLoRSdb VCF; anything else
# (e.g. BND) is dropped. Colors come from the shared svColor() palette.
KEEP_TYPES = {"DEL", "INS", "DUP", "INV"}


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


def _int(val, default=0):
    try:
        return int(str(val).split(",")[0])
    except (ValueError, TypeError):
        return default


def _float(val, default=0.0):
    try:
        return float(str(val).split(",")[0])
    except (ValueError, TypeError):
        return default


def main():
    if len(sys.argv) < 4:
        sys.exit("usage: lrSvColorsDbSvVcfToBed.py input.vcf.gz output.bed chrom.sizes")
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
    dropped_chrom = 0
    dropped_no_svtype = 0

    with opener(in_file, "rt") as fin, open(out_file, "w") as fout:
        for line in fin:
            if line.startswith("#"):
                continue
            f = line.rstrip("\n").split("\t")
            chrom, pos, vid, ref, alt, qual, filt, info = f[:8]

            if chrom not in chrom_sizes:
                dropped_chrom += 1
                continue

            info_d = parse_info(info)
            sv_raw = info_d.get("SVTYPE", "")
            sv_type = normalizeSvType(sv_raw)
            if sv_type not in KEEP_TYPES:
                dropped_no_svtype += 1
                continue

            start = int(pos) - 1  # VCF 1-based
            svlen_raw = _int(info_d.get("SVLEN", 0))
            abs_svlen = abs(svlen_raw)

            # pbsv doesn't always emit INFO/END. Derive the reference span:
            #   DEL: REF carries the anchor base + deleted bases; span = len(REF) - 1
            #   INS: pure insertion, 1 bp anchor; span = 1
            #   INV, DUP: SVLEN is the reference span
            if "END" in info_d:
                end = _int(info_d["END"])
            elif sv_type == "DEL":
                end = start + max(1, len(ref) - 1)
            elif sv_type == "INS":
                end = start + 1
            else:  # INV, DUP
                end = start + max(1, abs_svlen)
            if end <= start:
                end = start + 1

            svLen = end - start
            if sv_type == "INS":
                # For INS the inserted-sequence length is the ALT length
                # minus the anchor base (or fall back to abs(SVLEN)).
                ins_len = len(alt) - len(ref)
                if ins_len <= 0:
                    ins_len = abs_svlen
                insLen = ins_len
            else:
                insLen = 0

            ac = _int(info_d.get("AC", 0))
            an = _int(info_d.get("AN", 0))
            ns = _int(info_d.get("NS", 0))
            ac_hom = _int(info_d.get("AC_Hom", 0))
            ac_het = _int(info_d.get("AC_Het", 0))
            ac_hemi = _int(info_d.get("AC_Hemi", 0))
            nhomalt = _int(info_d.get("nhomalt", 0))
            af = _float(info_d.get("AF", 0.0))
            hwe = _float(info_d.get("HWE", 0.0))
            exchet = _float(info_d.get("ExcHet", 0.0))

            score = min(1000, max(0, int(round(af * 1000))))
            rgb = svColor(sv_type)

            featLen = insLen if sv_type == "INS" else svLen
            name = svName(sv_type, featLen, ac)

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
                str(ac),
                str(an),
                str(ns),
                str(ac_hom),
                str(ac_het),
                str(ac_hemi),
                str(nhomalt),
                f"{af:.6f}",
                f"{hwe:.6g}",
                f"{exchet:.6g}",
                ref,
                alt,
            ]
            fout.write("\t".join(row) + "\n")
            emitted += 1

    print(f"emitted rows:      {emitted}", file=sys.stderr)
    print(f"dropped bad chrom: {dropped_chrom}", file=sys.stderr)
    print(f"dropped svtype:    {dropped_no_svtype}", file=sys.stderr)


if __name__ == "__main__":
    main()
