#!/usr/bin/env python3
"""
TP53 VCEP Functional Preliminary PS3/BS3 track (primary subtrack of
Functional Evidence composite).

Drives colors from CSpec GN009 v2.4.0 Supplementary Table S3 &#8212; 4,193 missense
variants with pre-assigned PS3/BS3 codes integrating Kato, Funk, Giacomelli,
Kotler, and Kawaguchi assays.

bigBed 9+7 with filterValues on preliminaryCode.
"""

import argparse
import os
import re
import sys

import openpyxl

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/functionalAssays/prelim"
DEFAULT_SRC = "/hive/users/lrnassar/claude/RM37399/tp53_downloads/Functional-worksheet.xlsx"

PROT_RE = re.compile(r'^([A-Z])(\d+)([A-Z])$')

CLASSES = {
    'PS3':              ("180,0,0",    "+4", "PS3_Strong"),
    'PS3_Strong':       ("180,0,0",    "+4", "PS3_Strong"),
    'PS3_Moderate':     ("210,0,0",    "+2", "PS3_Moderate"),
    'PS3_Supporting':   ("245,152,152","+1", "PS3_Supporting"),
    'No evidence':      ("200,200,200","0",  "No evidence"),
    'BS3_Supporting':   ("213,247,213","-1", "BS3_Supporting"),
    'BS3':              ("0,210,0",    "-4", "BS3_Strong"),
    'BS3_Strong':       ("0,210,0",    "-4", "BS3_Strong"),
}

AUTOSQL = """table TP53FuncPrelim
"TP53 VCEP preliminary PS3/BS3 functional codes from CSpec GN009 v2.4.0 Table S3"
   (
   string chrom;            "Reference sequence chromosome or scaffold"
   uint   chromStart;       "Start position in chromosome"
   uint   chromEnd;         "End position in chromosome"
   string name;             "Missense change (e.g. R175H)"
   uint   score;            "Not used, all 0"
   char[1] strand;          "Not used, all ."
   uint   thickStart;       "Same as chromStart"
   uint   thickEnd;         "Same as chromEnd"
   uint   reserved;         "RGB color"
   string preliminaryCode;  "PS3_Strong / PS3_Moderate / PS3_Supporting / No evidence / BS3_Supporting / BS3_Strong"
   string points;           "Tavtigian points"
   string katoClass;        "Kato 2003 class (Non-functional/Partially/Functional/No data)"
   string giacomelliClass;  "Giacomelli 2018 LOF call"
   string kotlerClass;      "Kotler 2018 LOF call (DBD only)"
   string kawaguchiClass;   "Kawaguchi 2005 class (tetramerization)"
   string funkClass;        "Funk 2025 LOF call (exons 5-8)"
   lstring _mouseOver;      "HTML mouseover"
   )
"""


def parse_missense(p):
    if not isinstance(p, str):
        return None
    m = PROT_RE.match(p.strip())
    if not m:
        return None
    return m.group(1), int(m.group(2)), m.group(3)


def na(v):
    if v is None:
        return "No data"
    s = str(v).strip()
    if not s or s.upper() == 'NA':
        return "No data"
    return s


CODON72_CAVEAT = (
    "<br><b>Note:</b> Codon 72 in Table S3 uses the R72 haplotype (rs1042522) "
    "as background, not the NP_000537.3 Pro reference. Interpretation applies "
    "to variants on the R72 allele; clinicians should consider the haplotype "
    "when applying this data."
)


def _fmt_assay(class_label, raw_value, raw_label):
    """Combine class label with raw score (e.g., 'Non-functional (9.2% WT)')."""
    if raw_value in (None, "", "N/A"):
        return class_label
    if raw_label:
        return "{} ({}: {})".format(class_label, raw_label, raw_value)
    return "{} ({})".format(class_label, raw_value)


def mouseover(name, hgvsp, code, points, kato, giac, kotl, kaw, funk,
              codon, raw_scores):
    caveat = CODON72_CAVEAT if codon == 72 else ""
    rs = raw_scores.get(name, {})
    return (
        "<b>Preliminary &#8212; VCEP Table S3</b>"
        "<br><b>Variant:</b> p.{hgvsp} (NP_000537.3)"
        "<br><b>ACMG code:</b> {code} ({points} pts)"
        "<br><b>Contributing assays (class &#8212; raw score):</b>"
        "<br>&nbsp;&nbsp;Kato 2003: {kato}"
        "<br>&nbsp;&nbsp;Giacomelli 2018: {giac}"
        "<br>&nbsp;&nbsp;Kotler 2018: {kotl}"
        "<br>&nbsp;&nbsp;Kawaguchi 2005: {kaw}"
        "<br>&nbsp;&nbsp;Funk 2025: {funk}"
        "<br><b>Source:</b> CSpec GN009 v2.4.0 &#167;PS3/BS3"
        "{caveat}"
    ).format(
        name=name, hgvsp=hgvsp, code=code, points=points,
        kato=_fmt_assay(kato, rs.get('kato'),       "median % WT"),
        giac=_fmt_assay(giac, rs.get('giacomelli'), "etoposide Z"),
        kotl=kotl,  # Kotler has no raw score subtrack (Table S3 class only)
        kaw =_fmt_assay(kaw,  rs.get('kawaguchi'),  "OD state"),
        funk=_fmt_assay(funk, rs.get('funk'),       "RFS median"),
        caveat=caveat,
    )


def load_variants(src_xlsx):
    wb = openpyxl.load_workbook(src_xlsx, data_only=True)
    ws = wb["Supplementary Table S3"]
    out = []
    for row in ws.iter_rows(min_row=4, values_only=True):
        parsed = parse_missense(row[0])
        if not parsed:
            continue
        wt, codon, alt = parsed
        code_raw = str(row[7]).strip() if row[7] is not None else 'No evidence'
        out.append({
            'wt': wt, 'codon': codon, 'alt': alt,
            'short': "{}{}{}".format(wt, codon, alt),
            'hgvsp': "{}{}{}".format(wt, codon, alt),
            'kato': na(row[2]),
            'funk': na(row[3]),
            'giac': na(row[4]),
            'kotl': na(row[5]),
            'kaw': na(row[6]),
            'code': code_raw,
        })
    return out


def generate_bed(variants, tx, raw_scores):
    lines = []
    chrom = tx['chrom']
    for v in variants:
        code = v['code']
        if code not in CLASSES:
            code = 'No evidence'
        color, points, display_code = CLASSES[code]
        segs = lib.aa_codon_genomic(v['codon'], tx)
        if not segs:
            continue
        for g_start, g_end, _ex in segs:
            mo = mouseover(v['short'], v['hgvsp'], display_code, points,
                           v['kato'], v['giac'], v['kotl'], v['kaw'], v['funk'],
                           v['codon'], raw_scores)
            lines.append("\t".join([
                chrom, str(g_start), str(g_end),
                v['short'], "0", ".",
                str(g_start), str(g_end),
                color, display_code, points,
                v['kato'], v['giac'], v['kotl'], v['kaw'], v['funk'],
                mo,
            ]))
    return lines


def build(db, outdir, src_xlsx):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    tx = lib.get_transcript_info(db)
    variants = load_variants(src_xlsx)
    print("  {} missense variants".format(len(variants)))
    from collections import Counter
    print("  Distribution: {}".format(Counter(v['code'] for v in variants)))
    raw_scores = lib.load_per_paper_raw_scores()
    print("  Per-paper raw-score lookup: {} unique missense changes".format(
        len(raw_scores)))
    bed_lines = generate_bed(variants, tx, raw_scores)
    print("  {} BED rows".format(len(bed_lines)))

    as_file = os.path.join(outdir, "TP53FuncPrelim.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53FuncPrelim_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.bash("sort -k1,1 -k2,2n {0} -o {0}".format(bed))
    bb = os.path.join(outdir, "TP53FuncPrelim{}.bb".format(db.capitalize()))
    lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+7")
    print("  wrote {}".format(bb))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('-o', '--output-dir', default=DEFAULT_OUTDIR)
    p.add_argument('--db', action='append', help='hg38 or hg19 (repeat). Default hg38.')
    p.add_argument('--src', default=DEFAULT_SRC)
    args = p.parse_args()
    dbs = args.db if args.db else ['hg38']
    for db in dbs:
        build(db, args.output_dir, args.src)


if __name__ == "__main__":
    main()
