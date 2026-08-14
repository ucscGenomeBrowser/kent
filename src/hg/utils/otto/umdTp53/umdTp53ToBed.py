#!/usr/bin/env python3
"""
Build hg19 and hg38 BED files for the UMD TP53 variant track from the
UMD p53.fr database (https://p53.fr/download-the-database).

Rows come from UMD_variants_US.tsv (one row per unique TP53 variant).
hg38 coordinates are pulled from UMD_mutations_US.tsv via a cDNA_variant join,
since the variants file ships only hg19/hg18 coordinates.
"""

import argparse
import html
import logging
import sys
import unicodedata
from pathlib import Path


def normalize_text(s):
    """Fold non-ASCII characters to display-safe ASCII for BED extra fields.

    The source TSVs are MacRoman-encoded, and a handful of values in
    Comment_2_Activity contain MacRoman NBSP (0xCA) and `a` with grave (0x88,
    a typo). The UCSC Genome Browser does not transcode UTF-8 from bigBed
    extra fields, so any non-ASCII byte renders as mojibake. We strip combining
    marks (NFKD) and fall back to a numeric HTML entity for anything that
    can't be ASCII-decomposed.
    """
    if not s:
        return s
    out = []
    for ch in s:
        if ord(ch) <= 0x7F:
            out.append(ch)
            continue
        # Decompose so e.g. NBSP -> ' ', 'à' -> 'a' + combining grave
        decomposed = unicodedata.normalize("NFKD", ch)
        stripped = "".join(c for c in decomposed if ord(c) <= 0x7F and not unicodedata.combining(c))
        if stripped:
            out.append(stripped)
        else:
            out.append(f"&#{ord(ch)};")
    return "".join(out)


# Pathogenicity normalization + RGB colors.
# The source file uses 5 author-defined classes plus a few error/empty values.
PATHOGENICITY_RGB = {
    "Pathogenic":          "212,42,42",
    "Likely Pathogenic":   "230,90,50",
    "Possibly pathogenic": "247,148,29",
    "VUS":                 "230,205,0",
    "Benign":              "40,140,80",
    "Unknown":             "160,160,160",
}

# Map raw source values to the canonical class.
PATHOGENICITY_NORMALIZE = {
    "Pathogenic":          "Pathogenic",
    "Likely Pathogenic":   "Likely Pathogenic",
    "Possibly pathogenic": "Possibly pathogenic",
    "Possibly Pathogenic": "Possibly pathogenic",  # case-typo in source
    "VUS":                 "VUS",
    "Benign":              "Benign",
    "":                    "Unknown",
    "#VALUE!":             "Unknown",
    "#N/A":                "Unknown",
}

# Map source Variant_Classification to a smaller set of canonical labels for
# the filter dropdown. The source has 25 distinct values including isoform-tagged
# variants ("Missense_Mutation (isoforms beta)") and a lowercase typo.
def normalize_variant_classification(raw):
    if not raw:
        return "Other"
    base = raw.split("(")[0].strip().lower()
    mapping = {
        "missense_mutation":              "Missense",
        "missense_mutation_initiation_codon": "Missense",
        "nonsense_mutation":              "Nonsense",
        "synonymous_mutation":            "Synonymous",
        "frame_shift_del":                "Frameshift_Del",
        "frame_shift_del_complex":        "Frameshift_Del",
        "frame_shift_ins":                "Frameshift_Ins",
        "frameshift_indel":               "Frameshift_Indel",
        "in_frame_del":                   "In_frame_Del",
        "in_frame_ins":                   "In_frame_Ins",
        "deletion_complex":               "Complex_Del",
        "nonstop_mutation":               "Nonstop",
        "splice_site":                    "Splice",
        "intronic_mutation":              "Intronic",
        "3'utr":                          "UTR",
        "5'utr":                          "UTR",
        "3'flank":                        "Flank",
        "5'flank":                        "Flank",
        "gene_deletion":                  "Gene_Deletion",
    }
    return mapping.get(base, "Other")


def strip_lrg_prefix(protein_change):
    """LRG_321p1:p.R175H -> p.R175H. Empty string stays empty."""
    if not protein_change:
        return ""
    if ":" in protein_change:
        return protein_change.split(":", 1)[1]
    return protein_change


import re

_EVENT_RE = re.compile(r"^(c\.[^\s]+?(?:delins|del|ins|dup))([ACGTN]+)", re.IGNORECASE)


def shorten_name(cdna_variant):
    """BED name column is capped at 255 chars. Multi-kb deletions/insertions in
    HGVS names embed the full deleted/inserted sequence; collapse those to
    `<event>[<N>bp]` so the displayed label stays readable.

    Returns (display_name, was_truncated). The cap is 31 chars to stay within
    the ixIxx trix word-length limit, so the displayed name is also indexable.
    """
    if len(cdna_variant) <= 31:
        return cdna_variant, False
    m = _EVENT_RE.match(cdna_variant)
    if m:
        prefix, seq = m.group(1), m.group(2)
        shortened = f"{prefix}[{len(seq)}bp]"
        if len(shortened) <= 31:
            return shortened, True
    # Fallback: hard truncate to 28 + ellipsis
    return cdna_variant[:28] + "...", True


def read_tsv(path):
    """Read a UMD TSV. Yields list-of-fields rows.

    The files are MacRoman-encoded (classic Mac origin, CR line terminators).
    MacRoman is a single-byte encoding so the decode never fails.
    """
    with open(path, "rb") as fh:
        raw = fh.read()
    text = raw.decode("mac_roman").replace("\r\n", "\n").replace("\r", "\n")
    lines = text.split("\n")
    for line in lines:
        if not line:
            continue
        yield line.split("\t")


def build_mutations_hg38_lookup(mutations_tsv):
    """cDNA_variant -> (hg38_start, hg38_end). First non-? row wins.

    The mutations file occasionally records two slightly different end coords
    across patient rows for the same variant (off-by-one in End). For our
    purposes we keep the first valid pair; downstream coord validation lives in
    its own QA step, not here.
    """
    lookup = {}
    rows = read_tsv(mutations_tsv)
    next(rows)  # header
    # mutations TSV column order (1-based):
    #   $10 HG38_Start, $11 HG38_End, $22 cDNA_variant
    for fields in rows:
        if len(fields) < 22:
            continue
        cdna = fields[21]
        hg38_start = fields[9]
        hg38_end = fields[10]
        if not cdna or not hg38_start.isdigit() or not hg38_end.isdigit():
            continue
        if cdna in lookup:
            continue
        lookup[cdna] = (int(hg38_start), int(hg38_end))
    return lookup


def to_bed_interval(start, end):
    """1-based inclusive (start, end) (possibly end<start for minus-strand delins)
       -> 0-based half-open BED interval (bedStart, bedEnd)."""
    lo, hi = (start, end) if start <= end else (end, start)
    return lo - 1, hi


def format_float(s):
    """Pass through if it parses as float, else empty string."""
    try:
        return f"{float(s):g}"
    except (ValueError, TypeError):
        return ""


def format_uint(s):
    try:
        v = int(float(s))
        return str(v) if v >= 0 else "0"
    except (ValueError, TypeError):
        return "0"


def build_mouseover(name, protein_change, pathogenicity, var_class,
                    tumor_freq, records, activity_comment):
    """Compose the _mouseOver HTML field. Field is rendered as-is in the hover.

    One field per line, bold field labels.
    """
    var_text = html.escape(name)
    if protein_change:
        var_text += f" ({html.escape(protein_change)})"

    lines = [
        f"<b>Var:</b> {var_text}",
        f"<b>Class:</b> {html.escape(pathogenicity)}",
    ]
    if var_class:
        lines.append(f"<b>Var class:</b> {html.escape(var_class)}")
    if tumor_freq:
        lines.append(f"<b>Tumor freq:</b> {html.escape(tumor_freq)}% ({html.escape(records)} records)")
    if activity_comment:
        lines.append(f"<b>Activity:</b> {html.escape(activity_comment)}")
    return "<br>".join(lines)


def parse_variants_row(fields):
    """Pull the columns we need out of a variants-file row.

    Variants TSV column indices (1-based, from the readme + scan):
      $1   cDNA_variant
      $2   UMD_ID
      $3   COSMIC_ID
      $4   SNP_ID
      $37  P1 TP53 alpha protein change (with LRG_321p1: prefix)
      $49  HG19_Start
      $50  HG19_End
      $73  Variant_Classification
      $74  Variant_Type
      $75  Mutation_Type
      $77  Domain
      $78  Structure
      $79  PTM
      $80  Records_Number
      $83  Tumor_Freq        (percentage)
      $84  Cell_line_Freq
      $85  Somatic_Freq 2
      $86  Germline_Freq 2
      $95..$102  per-promoter activity % (WAF1, MDM2, BAX, 14-3-3s, AIP, GADD45, NOXA, p53R2)
      $103 Sift_Prediction
      $104 Sift_Score
      $105 Polyphen-2_HumVar
      $106 Polyphen-2_HumDiv
      $107 Mutassessor_prediction
      $108 Mutassessor_score
      $109 Provean_prediction
      $110 Provean_Score
      $111 Condel
      $112 Condel_Score
      $113 MutPred_Splice_General_Score
      $114 MutPred_Splice_Prediction_Label
      $115 MutPred_Splice_Confident_Hypotheses
      $116 Comment_1_Frequency
      $117 Comment_2_Activity
      $120 Comment_5_Outliers
      $121 Comment_6_Splicing
      $124 Pathogenicity
      $125 Final comment
    """
    if len(fields) < 125:
        return None

    def g(idx):
        return fields[idx - 1].strip() if idx - 1 < len(fields) else ""

    return {
        "cdna": g(1),
        "umdId": g(2),
        "cosmicId": g(3),
        "snpId": g(4),
        "proteinChange": strip_lrg_prefix(g(37)),
        "hg19_start_raw": g(49),
        "hg19_end_raw": g(50),
        "variantClassRaw": g(73),
        "variantType": g(74),
        "domain": g(77),
        "structure": g(78),
        "ptm": g(79),
        "recordsNumber": g(80),
        "tumorFreq": g(83),
        "cellLineFreq": g(84),
        "somaticFreq": g(85),
        "germlineFreq": g(86),
        "waf1Pct": g(95),
        "mdm2Pct": g(96),
        "baxPct": g(97),
        "p14333sPct": g(98),
        "aipPct": g(99),
        "gadd45Pct": g(100),
        "noxaPct": g(101),
        "p53r2Pct": g(102),
        "siftPrediction": g(103),
        "siftScore": g(104),
        "polyphenHumVar": g(105),
        "polyphenHumDiv": g(106),
        "mutAssessorPrediction": g(107),
        "mutAssessorScore": g(108),
        "proveanPrediction": g(109),
        "proveanScore": g(110),
        "condel": g(111),
        "condelScore": g(112),
        "mutpredSpliceScore": g(113),
        "mutpredSplicePrediction": g(114),
        "mutpredSpliceHypotheses": g(115),
        "commentFrequency": g(116),
        "activityComment": g(117),
        "commentOutliers": g(120),
        "commentSplicing": g(121),
        "pathogenicityRaw": g(124),
        "finalComment": g(125),
    }


def to_bed_record(row, chromStart, chromEnd):
    """Build the tab-joined BED+ output line for one assembly."""
    pathogenicity = PATHOGENICITY_NORMALIZE.get(row["pathogenicityRaw"], "Unknown")
    rgb = PATHOGENICITY_RGB[pathogenicity]
    var_class = normalize_variant_classification(row["variantClassRaw"])
    tumor_freq_raw = format_float(row["tumorFreq"])
    tumor_freq_bed = tumor_freq_raw or "0"
    records = format_uint(row["recordsNumber"])
    display_name, _ = shorten_name(row["cdna"])

    # Only show the tumor-frequency mouseover line when the value is actually
    # nonzero — most non-tumour variants report 0 and the line is just noise.
    tumor_freq_for_mouseover = ""
    try:
        if tumor_freq_raw and float(tumor_freq_raw) > 0:
            tumor_freq_for_mouseover = tumor_freq_raw
    except ValueError:
        pass

    mouseover = build_mouseover(
        name=display_name,
        protein_change=row["proteinChange"],
        pathogenicity=pathogenicity,
        var_class=var_class,
        tumor_freq=tumor_freq_for_mouseover,
        records=records,
        activity_comment=row["activityComment"],
    )

    columns = [
        "chr17",                 # chrom
        str(chromStart),         # chromStart
        str(chromEnd),           # chromEnd
        display_name,            # name (truncated for very long HGVS strings)
        "0",                     # score
        "-",                     # strand (TP53 is on minus strand)
        str(chromStart),         # thickStart
        str(chromEnd),           # thickEnd
        rgb,                     # reserved (itemRgb)
        row["cdna"],             # cDnaFull (original HGVS — same as name when not shortened)
        row["proteinChange"],
        pathogenicity,
        var_class,
        row["variantType"],
        row["domain"],
        row["structure"],
        row["ptm"],
        row["umdId"],
        row["cosmicId"],
        row["snpId"],
        row["activityComment"],
        format_float(row["waf1Pct"]),
        format_float(row["mdm2Pct"]),
        format_float(row["baxPct"]),
        format_float(row["p14333sPct"]),
        format_float(row["aipPct"]),
        format_float(row["gadd45Pct"]),
        format_float(row["noxaPct"]),
        format_float(row["p53r2Pct"]),
        tumor_freq_bed,
        format_float(row["cellLineFreq"]),
        format_float(row["somaticFreq"]),
        format_float(row["germlineFreq"]),
        records,
        row["siftScore"],
        row["siftPrediction"],
        row["polyphenHumVar"],
        row["polyphenHumDiv"],
        row["mutAssessorScore"],
        row["mutAssessorPrediction"],
        row["proveanScore"],
        row["proveanPrediction"],
        row["condel"],
        row["condelScore"],
        row["mutpredSpliceScore"],
        row["mutpredSplicePrediction"],
        row["mutpredSpliceHypotheses"],
        row["commentFrequency"],
        row["commentOutliers"],
        row["commentSplicing"],
        row["finalComment"],
        mouseover,
    ]
    # Disallow tabs and newlines inside field values; ASCII-fold non-ASCII.
    cleaned = [normalize_text(c.replace("\t", " ").replace("\n", " ").replace("\r", " "))
               for c in columns]
    return "\t".join(cleaned)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--variants", required=True, help="UMD_variants_US.tsv path")
    ap.add_argument("--mutations", required=True, help="UMD_mutations_US.tsv path (for hg38 coords)")
    ap.add_argument("--out-hg19", required=True, help="Output BED for hg19")
    ap.add_argument("--out-hg38", required=True, help="Output BED for hg38")
    args = ap.parse_args()

    logging.basicConfig(format="%(asctime)s %(levelname)s %(message)s",
                        level=logging.INFO)

    logging.info("Building hg38 lookup from %s", args.mutations)
    hg38_lookup = build_mutations_hg38_lookup(args.mutations)
    logging.info("Loaded %d hg38 entries", len(hg38_lookup))

    rows = read_tsv(args.variants)
    next(rows)  # header

    n_total = 0
    n_skipped_coords = 0
    n_missing_hg38 = 0
    n_written = 0

    with open(args.out_hg19, "w") as fh19, open(args.out_hg38, "w") as fh38:
        for fields in rows:
            n_total += 1
            row = parse_variants_row(fields)
            if row is None:
                n_skipped_coords += 1
                continue

            hg19_s_raw = row["hg19_start_raw"]
            hg19_e_raw = row["hg19_end_raw"]
            if not hg19_s_raw.isdigit() or not hg19_e_raw.isdigit():
                n_skipped_coords += 1
                continue

            hg19_start, hg19_end = to_bed_interval(int(hg19_s_raw), int(hg19_e_raw))
            fh19.write(to_bed_record(row, hg19_start, hg19_end) + "\n")

            hg38_pair = hg38_lookup.get(row["cdna"])
            if hg38_pair is None:
                n_missing_hg38 += 1
                continue

            hg38_start, hg38_end = to_bed_interval(*hg38_pair)
            fh38.write(to_bed_record(row, hg38_start, hg38_end) + "\n")
            n_written += 1

    logging.info("Total variants in source: %d", n_total)
    logging.info("Skipped (non-numeric coords): %d", n_skipped_coords)
    logging.info("Written to hg19: %d", n_total - n_skipped_coords)
    logging.info("Written to hg38: %d (missing hg38: %d)", n_written, n_missing_hg38)


if __name__ == "__main__":
    main()
