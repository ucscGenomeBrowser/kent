#!/usr/bin/env python3
"""
TP53 VCEP PVS1 Regions track generator.

Builds bigBed 9+4 for the three PVS1 decision-tree zones defined in
ClinGen CSpec GN009 v2.4.0 &#167;PVS1 for TP53 (NM_000546.6, chr17 minus strand):

    NMD         aa 1-350   PVS1            (+8 pts)  dark red
    PVS1_Strong aa 351-355 PVS1_Strong     (+4 pts)  red
    PVS1_Moderate aa 356-393 PVS1_Moderate (+2 pts)  orange

(Unlike the MMR genes in the InSiGHT hub, there is no trailing PVS1_n.a.
 region for TP53 &#8212; the entire coding region is PVS1-relevant.)
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/pvs1"

# (zone_label, aa_start, aa_end, acmg_code, points, color, rationale)
ZONES = [
    ("NMD",           1,   350, "PVS1",          "+8", "180,0,0",
     "Predicted NMD (PTC upstream of p.Lys351)"),
    ("PVS1_Strong",   351, 355, "PVS1_Strong",   "+4", "210,0,0",
     "Not NMD but >10% of protein removed (p.Lys351-p.Ala355)"),
    ("PVS1_Moderate", 356, 393, "PVS1_Moderate", "+2", "204,102,0",
     "Not NMD, <10% removed, C-terminal role unknown (p.Gly356-p.Asp393)"),
]

AUTOSQL = """table TP53PVS1
"TP53 VCEP PVS1 decision-tree regions (NM_000546.6)"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "Zone label (NMD, PVS1_Strong, PVS1_Moderate)"
   uint   score;       "Not used, all 0"
   char[1] strand;     "Not used, all ."
   uint   thickStart;  "Same as chromStart"
   uint   thickEnd;    "Same as chromEnd"
   uint   reserved;    "RGB color"
   string acmgCode;    "ACMG code at this zone"
   string points;      "Tavtigian points contribution"
   string rationale;   "Rationale from CSpec PVS1 decision tree"
   lstring _mouseOver; "HTML mouseover"
   )
"""


def mouseover(label, acmg, points, rationale, aa_lo, aa_hi):
    return (
        "<b>PVS1 zone:</b> {label} ({points} pts)"
        "<br><b>ACMG code:</b> {acmg}"
        "<br><b>Amino acids:</b> {lo}-{hi}"
        "<br><b>Rationale:</b> {rationale}"
        "<br><b>Transcript:</b> NM_000546.6 (NP_000537.3)"
    ).format(label=label, acmg=acmg, points=points,
             rationale=rationale, lo=aa_lo, hi=aa_hi)


def generate_bed(tx):
    lines = []
    chrom = tx['chrom']
    for label, aa_lo, aa_hi, acmg, points, color, rationale in ZONES:
        mo = mouseover(label, acmg, points, rationale, aa_lo, aa_hi)
        for g_start, g_end, _ex in lib.aa_to_genomic(aa_lo, aa_hi, tx):
            if g_start >= g_end:
                continue
            lines.append("\t".join([
                chrom, str(g_start), str(g_end),
                label, "0", ".",
                str(g_start), str(g_end),
                color,
                acmg, points, rationale, mo,
            ]))
    return lines


def build(db, outdir):
    print("=== {} ===".format(db))
    tx = lib.get_transcript_info(db)
    bed_lines = generate_bed(tx)
    print("  {} BED rows".format(len(bed_lines)))

    os.makedirs(outdir, exist_ok=True)
    as_file = os.path.join(outdir, "TP53PVS1.as")
    lib.write_autosql(as_file, AUTOSQL)

    bed = os.path.join(outdir, "TP53PVS1_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.bash("sort -k1,1 -k2,2n {0} -o {0}".format(bed))

    bb = os.path.join(outdir, "TP53PVS1{}.bb".format(db.capitalize()))
    lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+4")
    print("  wrote {}".format(bb))


def main():
    import argparse
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('-o', '--output-dir', default=DEFAULT_OUTDIR)
    p.add_argument('--db', action='append', help='hg38 or hg19 (repeat). Default hg38.')
    args = p.parse_args()
    dbs = args.db if args.db else ['hg38']
    for db in dbs:
        build(db, args.output_dir)


if __name__ == "__main__":
    main()
