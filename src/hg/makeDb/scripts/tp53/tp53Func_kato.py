#!/usr/bin/env python3
"""
TP53 VCEP per-paper functional subtrack: Kato et al. 2003 yeast transactivation.

Source: NCI TP53 Database R21 `FunctionIshiokaDownload_r21.csv` &#8212; Ishioka
yeast-based transactivation assay across 8 p53 target promoters. The median
across promoters drives the VCEP's 3-class call per CSpec GN009 v2.4.0:
    Non-functional:       median <= 20%
    Partially functional: 20% < median <= 75%
    Functional:           median > 75%  (supertransactivators also = Functional)

bigBed 9+6; hidden by default under the Functional Evidence composite.
"""

import argparse
import csv
import os
import re
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/functionalAssays/kato"
DEFAULT_SRC = "/hive/users/lrnassar/claude/RM37399/tp53_downloads/kato_FunctionIshioka_r21.csv"

PROMOTER_COLS = ['WAF1nWT', 'MDM2nWT', 'BAXnWT', 'h1433snWT',
                 'AIP1nWT', 'GADD45nWT', 'NOXAnWT', 'P53R2nWT']

AA_CHANGE_RE = re.compile(r'^([A-Z])(\d+)([A-Z])$')

CLASSES = {
    'Non-functional':       ("180,0,0",     "Non-functional"),   # dark red
    'Partially functional': ("245,152,152", "Partially functional"),  # pink
    'Functional':           ("0,210,0",     "Functional"),       # green
}

AUTOSQL = """table TP53FuncKato
"TP53 Kato et al. 2003 yeast transactivation (8 promoters) &#8212; NCI TP53 DB R21"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "Missense change (e.g., R175H)"
   uint   score;       "Not used, all 0"
   char[1] strand;     "Not used, all ."
   uint   thickStart;  "Same as chromStart"
   uint   thickEnd;    "Same as chromEnd"
   uint   reserved;    "RGB color"
   string katoClass;   "Non-functional / Partially functional / Functional"
   string medianPct;   "Median transactivation %, across 8 yeast promoters"
   uint   codon;       "Codon position on NP_000537.3"
   string promoters;   "Per-promoter % activity (8 values)"
   string pmid;        "Pubmed ID"
   lstring _mouseOver; "HTML mouseover"
   )
"""


def classify_median(m):
    if m <= 20.0:
        return 'Non-functional'
    if m <= 75.0:
        return 'Partially functional'
    return 'Functional'


def parse_aa(aa):
    m = AA_CHANGE_RE.match((aa or '').strip())
    if not m:
        return None
    return m.group(1), int(m.group(2)), m.group(3)


def mouseover(name, codon, cls, median_val, promoters):
    promoter_str = " | ".join(
        "{}: {:.1f}%".format(c, v) for c, v in promoters
    )
    return (
        "<b>Kato 2003 transactivation assay (yeast)</b>"
        "<br><b>Variant:</b> p.{name} (NP_000537.3, codon {codon})"
        "<br><b>Class:</b> {cls}"
        "<br><b>Median transactivation:</b> {median:.1f}% of wild type"
        "<br><b>Per-promoter:</b> {proms}"
        "<br><b>Thresholds:</b> Non-functional &le; 20%, Partially 20&ndash;75%, Functional &gt; 75%"
        "<br><b>Source:</b> NCI TP53 Database R21; "
        "<a href=\"https://pubmed.ncbi.nlm.nih.gov/12826609/\" target=\"_blank\">Kato 2003 (PMID:12826609)</a>"
    ).format(name=name, codon=codon, cls=cls,
             median=median_val, proms=promoter_str)


def load_variants(src_csv):
    out = []
    with open(src_csv) as f:
        reader = csv.DictReader(f)
        for row in reader:
            parsed = parse_aa(row.get('AAchange'))
            if not parsed:
                continue
            wt, codon, alt = parsed
            vals = []
            for c in PROMOTER_COLS:
                v = row.get(c)
                if v and v.strip():
                    try:
                        vals.append((c, float(v)))
                    except ValueError:
                        pass
            if not vals:
                continue
            median_val = statistics.median(v for _, v in vals)
            out.append({
                'wt': wt, 'codon': codon, 'alt': alt,
                'name': "{}{}{}".format(wt, codon, alt),
                'median': median_val,
                'promoters': vals,
                'class': classify_median(median_val),
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
        promoters_str = ";".join(
            "{}={:.1f}".format(c, val) for c, val in v['promoters']
        )
        for g_start, g_end, _ex in segs:
            mo = mouseover(v['name'], v['codon'], label,
                           v['median'], v['promoters'])
            lines.append("\t".join([
                chrom, str(g_start), str(g_end),
                v['name'], "0", ".",
                str(g_start), str(g_end),
                color, label,
                "{:.1f}".format(v['median']),
                str(v['codon']),
                promoters_str,
                "12826609",
                mo,
            ]))
    return lines


def build(db, outdir, src_csv):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    tx = lib.get_transcript_info(db)
    variants = load_variants(src_csv)
    print("  {} Kato variants parsed".format(len(variants)))
    from collections import Counter
    print("  Class distribution: {}".format(Counter(v['class'] for v in variants)))

    bed_lines = generate_bed(variants, tx)
    print("  {} BED rows".format(len(bed_lines)))
    as_file = os.path.join(outdir, "TP53FuncKato.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53FuncKato_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.run_sort_bed(bed)
    bb = os.path.join(outdir, "TP53FuncKato{}.bb".format(db.capitalize()))
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
