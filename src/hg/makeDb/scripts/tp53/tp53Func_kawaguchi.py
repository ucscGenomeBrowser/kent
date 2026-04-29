#!/usr/bin/env python3
"""
TP53 VCEP per-paper functional subtrack: Kawaguchi et al. 2005 oligomerization.

Source: NCI TP53 Database R21 `FunctionIshiokaDownload_r21.csv`,
`Oligomerisation_yeast` column. Assay is yeast-based oligomer formation for
the TP53 tetramerization domain (aa ~323-356). Per CSpec:
    Normal:   Tetramer (TETR)
    Abnormal: Monomer (MON) or Dimer (DIM)

Variants outside the OD have no data (NA) and are not emitted.

bigBed 9+5; hidden by default under the Functional Evidence composite.
"""

import argparse
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/functionalAssays/kawaguchi"
DEFAULT_SRC = "/hive/users/lrnassar/claude/RM37399/tp53_downloads/kato_FunctionIshioka_r21.csv"

AA_CHANGE_RE = re.compile(r'^([A-Z])(\d+)([A-Z])$')

# Raw column values &#8594; (class label, color)
OLIG_MAP = {
    'TETR': ('Normal (Tetramer)',   '0,210,0'),      # green
    'DIM':  ('Abnormal (Dimer)',    '245,152,152'),  # pink
    'MON':  ('Abnormal (Monomer)',  '180,0,0'),      # dark red
}

AUTOSQL = """table TP53FuncKawaguchi
"TP53 Kawaguchi et al. 2005 oligomerization assay (tetramerization domain only)"
   (
   string chrom;          "Reference sequence chromosome or scaffold"
   uint   chromStart;     "Start position in chromosome"
   uint   chromEnd;       "End position in chromosome"
   string name;           "Missense change (e.g., L344V)"
   uint   score;          "Not used, all 0"
   char[1] strand;        "Not used, all ."
   uint   thickStart;     "Same as chromStart"
   uint   thickEnd;       "Same as chromEnd"
   uint   reserved;       "RGB color"
   string oligomerState;  "Monomer / Dimer / Tetramer"
   string kawaguchiClass; "Normal (Tetramer) / Abnormal (Dimer) / Abnormal (Monomer)"
   uint   codon;          "Codon position on NP_000537.3"
   string pmid;           "Pubmed ID"
   lstring _mouseOver;    "HTML mouseover"
   )
"""


def parse_aa(aa):
    m = AA_CHANGE_RE.match((aa or '').strip())
    if not m:
        return None
    return m.group(1), int(m.group(2)), m.group(3)


def mouseover(name, codon, state, cls):
    return (
        "<b>Kawaguchi 2005 oligomerization assay (yeast)</b>"
        "<br><b>Variant:</b> p.{name} (NP_000537.3, codon {codon})"
        "<br><b>Oligomer state:</b> {state}"
        "<br><b>Class:</b> {cls}"
        "<br><b>Scope:</b> tetramerization domain (aa 323&ndash;356) only"
        "<br><b>Interpretation:</b> Tetramer = normal; Monomer/Dimer = abnormal"
        "<br><b>Source:</b> NCI TP53 Database R21; "
        "<a href=\"https://pubmed.ncbi.nlm.nih.gov/16007150/\" target=\"_blank\">Kawaguchi 2005 (PMID:16007150)</a>"
    ).format(name=name, codon=codon, state=state, cls=cls)


def load_variants(src_csv):
    out = []
    with open(src_csv) as f:
        reader = csv.DictReader(f)
        for row in reader:
            olig = (row.get('Oligomerisation_yeast') or '').strip()
            if olig not in OLIG_MAP:
                continue
            parsed = parse_aa(row.get('AAchange'))
            if not parsed:
                continue
            wt, codon, alt = parsed
            cls, color = OLIG_MAP[olig]
            state_full = {'TETR': 'Tetramer', 'DIM': 'Dimer', 'MON': 'Monomer'}[olig]
            out.append({
                'wt': wt, 'codon': codon, 'alt': alt,
                'name': "{}{}{}".format(wt, codon, alt),
                'state': state_full,
                'class': cls,
                'color': color,
            })
    return out


def generate_bed(variants, tx):
    lines = []
    chrom = tx['chrom']
    for v in variants:
        segs = lib.aa_codon_genomic(v['codon'], tx)
        if not segs:
            continue
        mo = mouseover(v['name'], v['codon'], v['state'], v['class'])
        for g_start, g_end, _ex in segs:
            lines.append("\t".join([
                chrom, str(g_start), str(g_end),
                v['name'], "0", ".",
                str(g_start), str(g_end),
                v['color'], v['state'], v['class'],
                str(v['codon']),
                "16007150",
                mo,
            ]))
    return lines


def build(db, outdir, src_csv):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    tx = lib.get_transcript_info(db)
    variants = load_variants(src_csv)
    print("  {} Kawaguchi variants parsed".format(len(variants)))
    from collections import Counter
    print("  Distribution: {}".format(Counter(v['state'] for v in variants)))
    bed_lines = generate_bed(variants, tx)
    print("  {} BED rows".format(len(bed_lines)))
    as_file = os.path.join(outdir, "TP53FuncKawaguchi.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53FuncKawaguchi_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.run_sort_bed(bed)
    bb = os.path.join(outdir, "TP53FuncKawaguchi{}.bb".format(db.capitalize()))
    lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+5")
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
