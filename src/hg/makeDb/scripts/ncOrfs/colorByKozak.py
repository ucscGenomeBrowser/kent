#!/usr/bin/env python3
"""
Color ncORF bigBed/bigGenePred features by Kozak translational efficiency.

Reads a bigBed of ncORFs (either bigGenePred 12+14 or BED12) on stdin or
from --in, fetches the 11-base Kozak context (positions -6..+5 relative to
the A of the start codon) from the genome 2bit, looks up the Noderer 2014
translational efficiency, and writes a bigGenePred BED with:
  - thickStart = chromStart, thickEnd = chromEnd  (full ORF as CDS)
  - itemRgb    = color from TE bin (Blue/Teal/Green/Orange/Red, grey if unknown)
  - three appended fields: startCodon, kozakStrength, kozakTE

Usage:
    bigBedToBed in.bb stdout | python3 colorByKozak.py [opts] > out.bed

Translation reference: ~/software/VuTR/pipeline/src/process_mane/smorfs.R
"""

import argparse
import sys
from pathlib import Path

import py2bit


# ---- Kozak helpers (from smorfs.R) ----
# Note: in the R source, get_context returns 11 bases at substr(seq, pos-6, pos+4),
# which spans Kozak positions -6, -5, -4, -3, -2, -1, +1, +2, +3, +4, +5.
# pos is 1-based and points to the A of ATG.

def get_context(seq, pos):
    """Return 11-base TIS context (Kozak -6..+5). pos is 1-based A-of-ATG.
    Returns None if length < 11."""
    if pos < 7:
        return None
    ctx = seq[pos - 7:pos + 4]
    return ctx if len(ctx) == 11 else None


def get_kozak_context(seq, start_site):
    """Return 7-base Kozak context (-3..+4). start_site is 1-based A-of-ATG."""
    if start_site < 4:
        return None
    ctx = seq[start_site - 4:start_site + 3]
    return ctx if len(ctx) == 7 else None


def get_kozak_consensus_strength(ctx7):
    """Categorical Strong/Moderate/Weak/None for a 7-base Kozak context."""
    if not ctx7 or len(ctx7) != 7:
        return "None"
    fb = ctx7[0].lower()
    lb = ctx7[6].lower()
    if fb in ("a", "g") and lb == "g":
        return "Strong"
    if fb in ("a", "g") or lb == "g":
        return "Moderate"
    return "Weak"


# ---- Color mapping ----
COLOR_BINS = (
    (0.5, "0,0,255"),       # Blue
    (0.6, "0,128,128"),     # Teal
    (0.7, "0,128,0"),       # Green
    (0.8, "255,165,0"),     # Orange
    (float("inf"), "255,0,0"),  # Red
)
GREY = "128,128,128"        # context unavailable / off-edge
PURPLE = "160,80,160"       # non-ATG start (Kozak rule does not apply)


def te_to_rgb(te):
    if te is None:
        return GREY
    for thresh, rgb in COLOR_BINS:
        if te < thresh:
            return rgb
    return COLOR_BINS[-1][1]


# ---- Reverse complement ----
_RC = str.maketrans("ACGTNacgtn", "TGCANtgcan")


def revcomp(s):
    return s.translate(_RC)[::-1]


# ---- TE table loader ----
def load_te_table(path):
    """Load Noderer 2014 TE table.

    File format expected (after smorfs te.sh processing):
        context\tefficiency\tlower_bound\tupper_bound  (header)
        gccgccatgg\t1.234\t...\n  (data)

    Also tolerant of the raw SD3 format (header + 11mer\tTE\t...).
    Keys are lowercased and U->T substituted.
    """
    table = {}
    with open(path) as fh:
        first = fh.readline()
        # Detect header row by looking for non-numeric in field 2
        parts = first.rstrip("\r\n").split("\t")
        looks_like_header = False
        if len(parts) >= 2:
            try:
                float(parts[1])
            except ValueError:
                looks_like_header = True
        if not looks_like_header and len(parts) >= 2:
            ctx = parts[0].lower().replace("u", "t")
            try:
                table[ctx] = float(parts[1])
            except ValueError:
                pass
        for line in fh:
            line = line.rstrip("\r\n")
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) < 2:
                continue
            ctx = parts[0].lower().replace("u", "t")
            try:
                table[ctx] = float(parts[1])
            except ValueError:
                continue
    if not table:
        sys.exit(f"ERROR: no TE rows loaded from {path}")
    # Quick sanity: keys should be 11 bases of ACGT
    sample = next(iter(table))
    if len(sample) != 11:
        sys.exit(f"ERROR: TE keys are length {len(sample)}, expected 11. "
                 f"Sample: {sample!r}")
    # Noderer 2014 SD3 reports TE rescaled so geometric-mean = 100
    # (raw range ~12-150, median ~79). Divide by 100 so the user-supplied
    # 0.5/0.6/0.7/0.8 thresholds are directly comparable.
    max_v = max(table.values())
    if max_v > 5:  # raw Noderer values are 12-150; if already 0-1 skip
        for k in table:
            table[k] /= 100.0
    return table


# ---- BED parsing / per-format synthesis to bigGenePred ----
# Output convention for ALL formats:
#   bed12 (12 fields) + 8 standard bigGenePred extras + format-specific extras
# Each synthesizer returns (bed12_list, biggenepred_extras_list, source_extras_list).
# Standard bigGenePred extras (in this order):
#   name2, cdsStartStat, cdsEndStat, exonFrames, type, geneName, geneName2, geneType

def _frames_for_blocks(block_count):
    return ",".join(["0"] * int(block_count)) + ","


def synth_biggenepred(fields):
    """Input is already bigGenePred 12+N: just split into bed12 / 8-bgp / source-extras."""
    if len(fields) < 20:
        raise ValueError(f"bigGenePred line has {len(fields)} fields, need >=20")
    bed12 = fields[:12]
    bgp = fields[12:20]
    extras = fields[20:]
    return bed12, bgp, extras


def synth_gencNcOrfBed12(fields):
    """gencNcOrfsPrimary/Comprehensive: bed12, name = 'orfId:GENE:biotype'."""
    if len(fields) < 12:
        raise ValueError("expected at least 12 fields for bed12 input")
    bed12 = list(fields[:12])
    raw_name = bed12[3]
    parts = raw_name.split(":")
    orf_id = parts[0]
    gene = parts[1] if len(parts) > 1 else ""
    biotype = parts[2] if len(parts) > 2 else ""
    bed12[3] = orf_id
    bgp = [
        gene,                              # name2
        "complete",                         # cdsStartStat
        "complete",                         # cdsEndStat
        _frames_for_blocks(bed12[9]),      # exonFrames
        biotype,                            # type
        "",                                # geneName (no ENSG available)
        gene,                              # geneName2
        biotype,                            # geneType
    ]
    return bed12, bgp, []


def synth_utrAnnotUorfsBed9(fields):
    """utrAnnotUorfs: bed 9 + 1 (uorfType). Synthesize single-block bed12."""
    if len(fields) < 10:
        raise ValueError("expected bed9+1 (10 fields) for utrAnnotUorfs")
    bed9 = list(fields[:9])
    uorfType = fields[9]
    size = int(bed9[2]) - int(bed9[1])
    bed12 = bed9 + ["1", f"{size},", "0,"]
    gene = bed12[3]
    bgp = [
        gene,                              # name2
        "complete",                         # cdsStartStat
        "complete",                         # cdsEndStat
        "0,",                              # exonFrames
        "uORF",                            # type
        "",                                # geneName
        gene,                              # geneName2
        "",                                # geneType
    ]
    return bed12, bgp, [uorfType]


def synth_utrAnnotUorfsBed12(fields):
    """utrAnnotUorfs after addIntrons.py: bed12 + 2 (uorfType, intronsSource)."""
    if len(fields) < 14:
        raise ValueError("expected bed12+2 (14 fields) for utrAnnotUorfsBed12")
    bed12 = list(fields[:12])
    uorfType = fields[12]
    intronsSource = fields[13]
    gene = bed12[3]
    bgp = [
        gene,                              # name2
        "complete",                         # cdsStartStat
        "complete",                         # cdsEndStat
        _frames_for_blocks(bed12[9]),      # exonFrames
        "uORF",                            # type
        "",                                # geneName
        gene,                              # geneName2
        "",                                # geneType
    ]
    return bed12, bgp, [uorfType, intronsSource]


def synth_metamorfBedORFtable(fields):
    """metamorf bedORFtable: bed12 + 5 (transcripts, rna_biotypes, cell_types,
    orf_annotations, kozak_contexts). Synthesize bigGenePred fields from
    transcripts/rna_biotypes."""
    if len(fields) < 17:
        # be lenient — pad missing
        fields = list(fields) + [""] * (17 - len(fields))
    bed12 = list(fields[:12])
    transcripts, rna_biotypes, cell_types, orf_annotations, kozak_contexts = fields[12:17]
    first_transcript = transcripts.split(",")[0] if transcripts else ""
    first_biotype = rna_biotypes.split(",")[0] if rna_biotypes else ""
    bgp = [
        "",                                # name2
        "complete",                         # cdsStartStat
        "complete",                         # cdsEndStat
        _frames_for_blocks(bed12[9]),      # exonFrames
        orf_annotations or "",              # type
        first_transcript,                   # geneName
        "",                                # geneName2
        first_biotype,                      # geneType
    ]
    return bed12, bgp, [transcripts, rna_biotypes, cell_types,
                        orf_annotations, kozak_contexts]


SYNTHESIZERS = {
    "bigGenePred":          synth_biggenepred,
    "gencNcOrfBed12":       synth_gencNcOrfBed12,
    "utrAnnotUorfsBed9":    synth_utrAnnotUorfsBed9,
    "utrAnnotUorfsBed12":   synth_utrAnnotUorfsBed12,
    "metamorf":             synth_metamorfBedORFtable,
}


def parse_bed_line(line, in_fmt):
    fields = line.rstrip("\n").split("\t")
    synth = SYNTHESIZERS.get(in_fmt)
    if synth is None:
        raise ValueError(f"Unknown input format: {in_fmt}")
    return synth(fields)


def get_genomic_context(tb, chrom, chrom_start, chrom_end, strand):
    """Return uppercase 11-base genomic context around the start codon, or None."""
    try:
        if strand == "+":
            seq = tb.sequence(chrom, chrom_start - 6, chrom_start + 5)
        elif strand == "-":
            seq = tb.sequence(chrom, chrom_end - 5, chrom_end + 6)
            seq = revcomp(seq)
        else:
            return None
    except (RuntimeError, ValueError):
        return None
    if seq is None or len(seq) != 11:
        return None
    return seq.upper()


def classify_start_codon(codon):
    if not codon or len(codon) != 3 or "N" in codon:
        return "none"
    codon = codon.upper()
    if codon in ("ATG", "CTG", "GTG", "TTG", "ACG"):
        return codon
    return "other"


def process(args):
    tb = py2bit.open(args.twoBit)
    te_table = load_te_table(args.teTable)

    if args.inFile and args.inFile != "-":
        in_fh = open(args.inFile)
    else:
        in_fh = sys.stdin

    if args.outFile and args.outFile != "-":
        out_fh = open(args.outFile, "w")
    else:
        out_fh = sys.stdout

    # Counters for the report
    counters = {
        "total": 0,
        "atg": 0,
        "non_atg": 0,
        "no_codon": 0,
        "ctx_unknown": 0,
        "te_miss": 0,
        "te_hit": 0,
        "color_blue": 0,
        "color_teal": 0,
        "color_green": 0,
        "color_orange": 0,
        "color_red": 0,
        "color_purple": 0,
        "color_grey": 0,
        "strand_unknown": 0,
    }
    by_strength = {"Strong": 0, "Moderate": 0, "Weak": 0, "non-ATG": 0, "None": 0}
    by_start = {}

    for raw in in_fh:
        if not raw.strip():
            continue
        bed12, bgp, source_extras = parse_bed_line(raw, args.inFmt)
        counters["total"] += 1

        chrom = bed12[0]
        chrom_start = int(bed12[1])
        chrom_end = int(bed12[2])
        strand = bed12[5] if len(bed12) > 5 else "."

        if strand not in ("+", "-"):
            counters["strand_unknown"] += 1
            ctx11 = None
            start_codon = "none"
        else:
            ctx11 = get_genomic_context(tb, chrom, chrom_start, chrom_end, strand)
            start_codon = ctx11[6:9] if ctx11 else "none"

        sc_label = classify_start_codon(start_codon)
        by_start[sc_label] = by_start.get(sc_label, 0) + 1
        if sc_label == "ATG":
            counters["atg"] += 1
        elif sc_label == "none":
            counters["no_codon"] += 1
        else:
            counters["non_atg"] += 1

        # Kozak strength label:
        #   ATG + valid context -> Strong/Moderate/Weak (Kozak rule on -3, +4)
        #   non-ATG with valid context -> "non-ATG" (rule does not apply)
        #   no/short context -> "None"
        if not ctx11 or len(ctx11) != 11:
            strength = "None"
            counters["ctx_unknown"] += 1
        elif sc_label != "ATG":
            strength = "non-ATG"
        else:
            ctx7 = ctx11[3:10]  # positions -3..+4 within the 11-mer
            strength = get_kozak_consensus_strength(ctx7)
        by_strength[strength] = by_strength.get(strength, 0) + 1

        # Numeric TE: only meaningful for ATG; non-ATG and missing -> -1.
        if sc_label == "ATG" and ctx11:
            te = te_table.get(ctx11.lower())
            if te is None:
                counters["te_miss"] += 1
                te_str = "-1"
                rgb = GREY
            else:
                counters["te_hit"] += 1
                te_str = f"{te:.4f}"
                rgb = te_to_rgb(te)
        elif sc_label != "ATG" and ctx11:
            # Non-ATG ORF with a valid context: show distinct purple color.
            te_str = "-1"
            rgb = PURPLE
        else:
            # No context (chromosome edge, etc.) -> grey
            te_str = "-1"
            rgb = GREY

        # Color counters
        if rgb == GREY:
            counters["color_grey"] += 1
        elif rgb == PURPLE:
            counters["color_purple"] += 1
        elif rgb == "0,0,255":
            counters["color_blue"] += 1
        elif rgb == "0,128,128":
            counters["color_teal"] += 1
        elif rgb == "0,128,0":
            counters["color_green"] += 1
        elif rgb == "255,165,0":
            counters["color_orange"] += 1
        elif rgb == "255,0,0":
            counters["color_red"] += 1

        # Force thickStart/thickEnd to span the entire ORF (full CDS)
        bed12[6] = str(chrom_start)
        bed12[7] = str(chrom_end)
        bed12[8] = rgb

        # Output: bed12 + 8 bigGenePred extras + format-specific extras + 3 Kozak fields
        out_fields = bed12 + bgp + list(source_extras) + [sc_label, strength, te_str]
        out_fh.write("\t".join(out_fields) + "\n")

    if in_fh is not sys.stdin:
        in_fh.close()
    if out_fh is not sys.stdout:
        out_fh.close()
    tb.close()

    # Write report
    if args.report:
        with open(args.report, "w") as r:
            r.write("# colorByKozak.py report\n")
            for k, v in counters.items():
                r.write(f"{k}\t{v}\n")
            r.write("\n# Kozak strength counts (categorical)\n")
            for k, v in sorted(by_strength.items()):
                r.write(f"strength_{k}\t{v}\n")
            r.write("\n# Start codon counts\n")
            for k, v in sorted(by_start.items()):
                r.write(f"startCodon_{k}\t{v}\n")

    # Always write a short summary to stderr
    sys.stderr.write(f"[colorByKozak] total={counters['total']} "
                     f"atg={counters['atg']} non_atg={counters['non_atg']} "
                     f"te_hit={counters['te_hit']} te_miss={counters['te_miss']} "
                     f"ctx_unknown={counters['ctx_unknown']}\n")
    sys.stderr.write(f"[colorByKozak] colors: "
                     f"red={counters['color_red']} "
                     f"orange={counters['color_orange']} "
                     f"green={counters['color_green']} "
                     f"teal={counters['color_teal']} "
                     f"blue={counters['color_blue']} "
                     f"purple={counters['color_purple']} "
                     f"grey={counters['color_grey']}\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="inFile", default="-",
                    help="input BED file (default: stdin)")
    ap.add_argument("--out", dest="outFile", default="-",
                    help="output BED file (default: stdout)")
    ap.add_argument("--twoBit", default="/hive/data/genomes/hg38/hg38.2bit",
                    help="genome 2bit file")
    ap.add_argument("--teTable", required=True,
                    help="Noderer 2014 TE table (TSV with header)")
    ap.add_argument("--inFmt",
                    choices=tuple(SYNTHESIZERS.keys()),
                    default="bigGenePred",
                    help="input BED flavor: %(choices)s")
    ap.add_argument("--report", help="optional path to write a TSV report")
    args = ap.parse_args()
    process(args)


if __name__ == "__main__":
    main()
