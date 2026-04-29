#!/usr/bin/env python3
"""
TP53 VCEP per-paper functional subtrack: Giacomelli et al. 2018
A549 saturation mutagenesis Z-scores.

Source: Giacomelli 2018 Supp Table 3 (Z-scores) &#8212; `giacomelli_ZScores_suppT3.xlsx`,
PMID 30224644. The VCEP uses the A549_p53NULL_Etoposide_Z-score column with
threshold <= -0.21 = LOF (loss-of-function).

bigBed 9+6; hidden by default under the Functional Evidence composite.
"""

import argparse
import os
import sys

import openpyxl

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/functionalAssays/giacomelli"
DEFAULT_SRC = "/hive/users/lrnassar/claude/RM37399/tp53_downloads/giacomelli_ZScores_suppT3.xlsx"

LOF_THRESHOLD = -0.21

CLASSES = {
    'LOF':   ("180,0,0",    "LOF"),         # dark red
    'noLOF': ("200,200,200", "noLOF"),     # gray
}

AUTOSQL = """table TP53FuncGiacomelli
"TP53 Giacomelli et al. 2018 A549 p53-NULL etoposide Z-scores (saturation mutagenesis)"
   (
   string chrom;              "Reference sequence chromosome or scaffold"
   uint   chromStart;         "Start position in chromosome"
   uint   chromEnd;           "End position in chromosome"
   string name;               "Missense change (e.g., R175H)"
   uint   score;              "Not used, all 0"
   char[1] strand;            "Not used, all ."
   uint   thickStart;         "Same as chromStart"
   uint   thickEnd;           "Same as chromEnd"
   uint   reserved;           "RGB color"
   string giacomelliClass;    "LOF (Z <= -0.21) / noLOF"
   string etoposideZ;         "A549 p53-NULL Etoposide Z-score"
   string nutlinWtZ;          "A549 p53-WT Nutlin-3 Z-score"
   string nutlinNullZ;        "A549 p53-NULL Nutlin-3 Z-score"
   uint   codon;              "Codon position on NP_000537.3"
   string pmid;               "Pubmed ID"
   lstring _mouseOver;        "HTML mouseover"
   )
"""


def safe_float(v):
    if v is None or v == '':
        return None
    try:
        return float(v)
    except (ValueError, TypeError):
        return None


def classify(z):
    if z is None:
        return None
    return 'LOF' if z <= LOF_THRESHOLD else 'noLOF'


def mouseover(name, codon, cls, z_etop, z_nutlin_wt, z_nutlin_null):
    return (
        "<b>Giacomelli 2018 A549 saturation mutagenesis (Z-scores)</b>"
        "<br><b>Variant:</b> p.{name} (NP_000537.3, codon {codon})"
        "<br><b>Class:</b> {cls} (VCEP threshold: Etoposide Z &le; &minus;0.21)"
        "<br><b>Etoposide Z-score:</b> {z_etop}"
        "<br><b>p53-WT Nutlin-3 Z:</b> {z_nw}"
        "<br><b>p53-NULL Nutlin-3 Z:</b> {z_nn}"
        "<br><b>Source:</b> "
        "<a href=\"https://pubmed.ncbi.nlm.nih.gov/30224644/\" target=\"_blank\">Giacomelli et al. 2018 (PMID:30224644)</a>, "
        "Supp Table 3"
    ).format(name=name, codon=codon, cls=cls,
             z_etop="{:.3f}".format(z_etop) if z_etop is not None else "N/A",
             z_nw="{:.3f}".format(z_nutlin_wt) if z_nutlin_wt is not None else "N/A",
             z_nn="{:.3f}".format(z_nutlin_null) if z_nutlin_null is not None else "N/A")


def load_variants(src_xlsx):
    wb = openpyxl.load_workbook(src_xlsx, data_only=True)
    ws = wb.active
    out = []
    for row in ws.iter_rows(min_row=3, values_only=True):
        allele = row[0]
        wt_aa = row[1]
        alt_aa = row[2]
        pos = row[3]
        if not allele or not isinstance(allele, str):
            continue
        if wt_aa == alt_aa or alt_aa in ('*', 'X', None):
            continue
        try:
            codon = int(pos)
        except (ValueError, TypeError):
            continue
        z_etop = safe_float(row[6])  # A549_p53NULL_Etoposide_Z-score
        z_nutlin_wt = safe_float(row[4])
        z_nutlin_null = safe_float(row[5])
        cls = classify(z_etop)
        if cls is None:
            continue
        out.append({
            'wt': wt_aa,
            'codon': codon,
            'alt': alt_aa,
            'name': "{}{}{}".format(wt_aa, codon, alt_aa),
            'z_etop': z_etop,
            'z_nutlin_wt': z_nutlin_wt,
            'z_nutlin_null': z_nutlin_null,
            'class': cls,
        })
    return out


def generate_bed(variants, tx):
    lines = []
    chrom = tx['chrom']
    for v in variants:
        color, label = CLASSES[v['class']]
        segs = lib.aa_codon_genomic(v['codon'], tx)
        if not segs:
            continue
        mo = mouseover(v['name'], v['codon'], label,
                       v['z_etop'], v['z_nutlin_wt'], v['z_nutlin_null'])
        for g_start, g_end, _ex in segs:
            lines.append("\t".join([
                chrom, str(g_start), str(g_end),
                v['name'], "0", ".",
                str(g_start), str(g_end),
                color, label,
                "{:.3f}".format(v['z_etop']),
                "{:.3f}".format(v['z_nutlin_wt']) if v['z_nutlin_wt'] is not None else "N/A",
                "{:.3f}".format(v['z_nutlin_null']) if v['z_nutlin_null'] is not None else "N/A",
                str(v['codon']),
                "30224644",
                mo,
            ]))
    return lines


def build(db, outdir, src_xlsx):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    tx = lib.get_transcript_info(db)
    variants = load_variants(src_xlsx)
    print("  {} Giacomelli variants parsed".format(len(variants)))
    from collections import Counter
    print("  LOF/noLOF: {}".format(Counter(v['class'] for v in variants)))
    bed_lines = generate_bed(variants, tx)
    print("  {} BED rows".format(len(bed_lines)))
    as_file = os.path.join(outdir, "TP53FuncGiacomelli.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53FuncGiacomelli_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.bash("sort -k1,1 -k2,2n {0} -o {0}".format(bed))
    bb = os.path.join(outdir, "TP53FuncGiacomelli{}.bb".format(db.capitalize()))
    lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+6")
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
