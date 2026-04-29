#!/usr/bin/env python3
"""
TP53 VCEP per-paper functional subtrack: Funk et al. 2025 CRISPR saturation
mutagenesis (HCT116, exons 5-8).

Source: Funk 2025 Supp Table 2 &#8212; per-variant RFS (relative fitness score).
PMID 39774325. VCEP threshold: RFS >= 0 is LOF; RFS < 0 is no-LOF (synonymous
baseline -1, nonsense baseline +1).

Scope: limited to TP53 exons 5-8 (aa 126-307). Missense variants only for this
subtrack.

bigBed 9+6; hidden by default under Functional Evidence composite.
"""

import argparse
import os
import re
import sys

import openpyxl

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/functionalAssays/funk"
DEFAULT_SRC = "/hive/users/lrnassar/claude/RM37399/tp53_downloads/funk_suppT1_11.xlsx"

LOF_THRESHOLD = 0.0
HGVS_P_RE = re.compile(r'^p\.([A-Z])(\d+)([A-Z])$')
HGVS_G_SNV_RE = re.compile(r'^NC_000017\.11:g\.(\d+)([ACGT])>([ACGT])$')

CLASSES = {
    'LOF':   ("180,0,0",    "LOF"),
    'noLOF': ("200,200,200", "noLOF"),
}

AUTOSQL = """table TP53FuncFunk
"TP53 Funk et al. 2025 CRISPR saturation mutagenesis RFS (HCT116, exons 5-8)"
   (
   string chrom;          "Reference sequence chromosome or scaffold"
   uint   chromStart;     "Start position in chromosome"
   uint   chromEnd;       "End position in chromosome"
   string name;           "Missense change (e.g., R175H)"
   uint   score;          "Not used, all 0"
   char[1] strand;        "Not used, all ."
   uint   thickStart;     "Same as chromStart"
   uint   thickEnd;       "Same as chromEnd"
   uint   reserved;       "RGB color"
   string funkClass;      "LOF (RFS >= 0) / noLOF"
   string rfsMedian;      "RFS median across 3 replicates"
   uint   codon;          "Codon position on NP_000537.3"
   string hgvsg;          "Genomic HGVS (hg38)"
   string pmid;           "Pubmed ID"
   lstring _mouseOver;    "HTML mouseover"
   )
"""


def safe_float(v):
    if v is None or v == '':
        return None
    try:
        return float(v)
    except (ValueError, TypeError):
        return None


def classify(rfs):
    if rfs is None:
        return None
    return 'LOF' if rfs >= LOF_THRESHOLD else 'noLOF'


def parse_hgvsp(p):
    m = HGVS_P_RE.match((p or '').strip())
    if not m:
        return None
    return m.group(1), int(m.group(2)), m.group(3)


def mouseover(name, codon, cls, rfs_median, hgvsg):
    return (
        "<b>Funk 2025 CRISPR saturation mutagenesis (HCT116, exons 5-8)</b>"
        "<br><b>Variant:</b> p.{name} (NP_000537.3, codon {codon})"
        "<br><b>Class:</b> {cls} (VCEP threshold: RFS &ge; 0)"
        "<br><b>RFS median:</b> {rfs} (synonymous baseline &minus;1, nonsense +1)"
        "<br><b>Genomic:</b> {g}"
        "<br><b>Source:</b> "
        "<a href=\"https://pubmed.ncbi.nlm.nih.gov/39774325/\" target=\"_blank\">Funk et al. 2025 (PMID:39774325)</a>, "
        "Supp Table 2"
    ).format(name=name, codon=codon, cls=cls,
             rfs="{:.3f}".format(rfs_median) if rfs_median is not None else "N/A",
             g=hgvsg or "N/A")


def load_variants(src_xlsx):
    wb = openpyxl.load_workbook(src_xlsx, data_only=True)
    ws = wb["Supp_Table_2"]
    out = []
    for row in ws.iter_rows(min_row=4, values_only=True):
        type_p = row[8]
        effect = row[9]
        rfs_median = safe_float(row[34])
        if type_p != 'mis':
            continue
        if rfs_median is None:
            continue
        parsed = parse_hgvsp(effect)
        if not parsed:
            continue
        wt, codon, alt = parsed
        cls = classify(rfs_median)
        out.append({
            'wt': wt,
            'codon': codon,
            'alt': alt,
            'name': "{}{}{}".format(wt, codon, alt),
            'rfs_median': rfs_median,
            'class': cls,
            'hgvsg': row[1] or '',
        })
    return out


def generate_bed(variants, tx):
    lines = []
    chrom = tx['chrom']
    # Dedupe by (codon, alt) keeping worst-case (most pathogenic) RFS
    # (multiple nucleotide paths may encode the same AA change).
    best = {}
    for v in variants:
        k = (v['codon'], v['alt'])
        cur = best.get(k)
        if cur is None or v['rfs_median'] > cur['rfs_median']:
            best[k] = v
    for k, v in sorted(best.items()):
        color, label = CLASSES[v['class']]
        segs = lib.aa_codon_genomic(v['codon'], tx)
        if not segs:
            continue
        mo = mouseover(v['name'], v['codon'], label,
                       v['rfs_median'], v['hgvsg'])
        for g_start, g_end, _ex in segs:
            lines.append("\t".join([
                chrom, str(g_start), str(g_end),
                v['name'], "0", ".",
                str(g_start), str(g_end),
                color, label,
                "{:.3f}".format(v['rfs_median']),
                str(v['codon']),
                v['hgvsg'],
                "39774325",
                mo,
            ]))
    return lines


def build(db, outdir, src_xlsx):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    tx = lib.get_transcript_info(db)
    variants = load_variants(src_xlsx)
    print("  {} Funk missense variants parsed".format(len(variants)))
    from collections import Counter
    print("  LOF/noLOF: {}".format(Counter(v['class'] for v in variants)))
    bed_lines = generate_bed(variants, tx)
    print("  {} BED rows".format(len(bed_lines)))
    as_file = os.path.join(outdir, "TP53FuncFunk.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53FuncFunk_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.bash("sort -k1,1 -k2,2n {0} -o {0}".format(bed))
    bb = os.path.join(outdir, "TP53FuncFunk{}.bb".format(db.capitalize()))
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
