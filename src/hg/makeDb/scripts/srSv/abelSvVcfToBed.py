#!/usr/bin/env python3
"""Convert an Abel et al. 2020 CCDG structural-variant VCF (Build38 or Build37)
to the bed14+ representation used by the abelSv track.

Inputs: gzipped VCF on stdin (pass through zcat).
Output: tab-separated bed rows on stdout.

Usage:
    zcat Build38.public.v2.vcf.gz | vcfToBed.py B38 > B38.bed
    zcat Build37.public.v2.vcf.gz | vcfToBed.py B37 > B37.prelift.bed
"""
import sys
import gzip
import os
import re

# Share the lrSv svName/normalizeSvType helpers without duplicating the module.
sys.path.insert(0, os.path.expanduser("~/kent/src/hg/makeDb/scripts/lrSv"))
from lrSvCommon import svName, normalizeSvType

# RGB colors per SV type (flat solid colors)
COLORS = {
    "DEL": "220,50,32",    # red
    "DUP": "0,120,200",    # blue
    "INV": "230,140,0",    # orange
    "MEI": "140,80,180",   # purple
    "BND": "120,120,120",  # grey
}

# Population codes present in INFO (per-ancestry AC/AN)
POPS = ["AFR", "AMR", "NFE", "FE", "EAS", "SAS", "PI", "Other"]

# Integer fields we extract and report as int
INT_FIELDS = ["AC", "AN", "NSAMP", "NFAM"]
FLOAT_FIELDS = ["MSQ", "AF"]


def parse_info(info_str):
    """Parse INFO column into dict; flags become True."""
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


BND_ALT_RE = re.compile(r"([\[\]])([^:]+):(\d+)[\[\]]")


def parse_bnd_alt(alt):
    """Extract mate chrom and position from a BND ALT string."""
    m = BND_ALT_RE.search(alt)
    if m:
        return m.group(2), int(m.group(3))
    return "", -1


def classify_svtype(svtype_raw):
    """Collapse SVTYPE string to one of DEL/DUP/INV/MEI/BND.
    The VCF has composite types like <DEL:ME:LINE|L1|L1HS> that should
    map to MEI (mobile-element-insertion-derived deletion)."""
    if svtype_raw == "BND":
        return "BND"
    if svtype_raw.startswith("DEL"):
        if ":ME" in svtype_raw:
            return "MEI"
        return "DEL"
    if svtype_raw == "MEI" or svtype_raw.startswith("INS:ME") or svtype_raw.startswith("ME"):
        return "MEI"
    if svtype_raw.startswith("DUP"):
        return "DUP"
    if svtype_raw.startswith("INV"):
        return "INV"
    return svtype_raw


def get_alt_svtype(alt):
    """ALT column often holds the detailed SV type for non-BND, e.g. <DEL:ME:LINE|L1|L1HS>."""
    alt = alt.strip()
    if alt.startswith("<") and alt.endswith(">"):
        return alt[1:-1]
    return ""


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("B38", "B37", "B37lift"):
        sys.exit("usage: vcfToBed.py B38|B37|B37lift")
    tag = sys.argv[1]
    # Will we need the chr-prefix? B37 uses bare names.
    add_chr = (tag == "B37")
    out_tag = "B37lift" if tag == "B37" else tag

    for line in sys.stdin:
        if line.startswith("#"):
            continue
        f = line.rstrip("\n").split("\t")
        chrom, pos, vid, ref, alt, qual, filt, info = f[:8]
        info_d = parse_info(info)

        # Skip secondary BND records; we keep only the primary.
        if info_d.get("SECONDARY"):
            continue

        svtype_raw = info_d.get("SVTYPE", "")
        # Also check ALT for richer subtype info
        alt_sv = get_alt_svtype(alt)
        svclass = classify_svtype(alt_sv or svtype_raw)

        if svclass not in COLORS:
            continue  # skip unknown types

        # Canonicalize via lrSvCommon (DEL/DUP/INV/MEI/BND are already canonical).
        svType = normalizeSvType(svclass)

        start = int(pos) - 1  # VCF is 1-based
        mate_chrom = ""
        mate_pos = -1
        sv_length = -1
        end = start + 1  # default single-width

        if svclass == "BND":
            mate_chrom, mate_pos = parse_bnd_alt(alt)
            if add_chr and mate_chrom and not mate_chrom.startswith("chr"):
                mate_chrom = "chr" + mate_chrom
            end = start + 1
        else:
            if "END" in info_d:
                end = int(info_d["END"])
            if "SVLEN" in info_d:
                try:
                    # SVLEN can be comma-list or negative
                    sv_length = int(info_d["SVLEN"].split(",")[0])
                except ValueError:
                    sv_length = -1
            if end <= start:
                end = start + 1

        if add_chr and not chrom.startswith("chr"):
            chrom = "chr" + chrom

        # Numeric fields
        ac = int(info_d.get("AC", "0").split(",")[0])
        an = int(info_d.get("AN", "0"))
        if "AF" in info_d:
            try:
                af = float(info_d["AF"].split(",")[0])
            except ValueError:
                af = (ac / an) if an else 0.0
        else:
            af = (ac / an) if an else 0.0
        try:
            msq = float(info_d.get("MSQ", "0"))
        except ValueError:
            msq = 0.0
        nsamp = int(info_d.get("NSAMP", "0"))
        nfam = int(info_d.get("NFAM", "0"))

        # Score for bigBed: scale AF to 0-1000
        score = min(1000, max(0, int(round(af * 1000))))

        # Per-population AC/AN (missing -> 0)
        pop_vals = []
        for pop in POPS:
            pop_ac = int(info_d.get(f"AC_{pop}", "0").split(",")[0])
            pop_an = int(info_d.get(f"AN_{pop}", "0"))
            pop_vals.append((pop_ac, pop_an))

        # svLen canonical = reference span; keep legacy abs(SVLEN) as the displayed
        # length too by writing it into svLen when non-BND (matches legacy abelSv
        # filter range). For BND, use -1 (a sentinel preserved in the as schema).
        if svclass == "BND":
            svLen = -1
            insLen = 0
        else:
            svLen = end - start
            if svclass == "MEI":
                # For MEI, abs(SVLEN) is the inserted-element size.
                # abelSv VCF reports MEI with negative SVLEN (e.g. -303),
                # so test != 0 rather than > 0.
                insLen = abs(sv_length) if sv_length != 0 and sv_length != -1 else 0
            else:
                insLen = 0

        rgb = COLORS[svclass]

        featLen = insLen if svType in ("INS", "MEI") else (svLen if svLen > 0 else None)
        name = svName(svType, featLen, ac)

        row = [
            chrom, str(start), str(end),
            name,
            str(score),
            ".",
            str(start), str(end),
            rgb,
            svType,
            str(svLen),
            str(insLen),
            str(ac),
            out_tag,
            filt,
            str(an),
            f"{af:.6f}",
            f"{msq:.2f}",
            str(nsamp), str(nfam),
            mate_chrom, str(mate_pos),
        ]
        # Append per-pop AC/AN in the order: AFR, AMR, NFE, FE, EAS, SAS, PI, Other
        for ac_p, an_p in pop_vals:
            row.append(str(ac_p))
            row.append(str(an_p))

        print("\t".join(row))


if __name__ == "__main__":
    main()
