#!/usr/bin/env python3
"""
TP53 VCEP PVS1 Splice Sites subtrack (under PVS1 Evidence composite).

Parses Supplementary Table S1 (1,061 rows) from the CSpec GN009 v2.4.0
splicing worksheet. Extracts the 120 canonical +/- 1,2 splice-site SNVs,
forward-fills PVS1 strength assignments from the anchor variant to its
two sibling variants at the same position, and emits bigBed 9+6.

Transcript: NM_000546.6 (chr17 minus strand).
"""

import argparse
import os
import re
import sys

import openpyxl

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/pvs1Splice"
DEFAULT_SRC = "/hive/users/lrnassar/claude/RM37399/tp53_downloads/splicing_worksheet.xlsx"

VAR_RE = re.compile(r'^c\.(-?\d+)([+-])([12])([ACGT])>([ACGT])$')

# (color, points, acmg_for_display). RNA-variants use same color as canonical.
STRENGTH_MAP = {
    "PVS1":               ("180,0,0",  "+8", "PVS1"),
    "PVS1 (RNA)":         ("180,0,0",  "+8", "PVS1 (RNA)"),
    "PVS1_Strong":        ("210,0,0",  "+4", "PVS1_Strong"),
    "PVS1_Strong (RNA)":  ("210,0,0",  "+4", "PVS1_Strong (RNA)"),
    "PVS1_Moderate":      ("204,102,0","+2", "PVS1_Moderate"),
    "PVS1_Moderate (RNA)":("204,102,0","+2", "PVS1_Moderate (RNA)"),
    "PVS1_N/A":           ("128,128,128","0", "PVS1_N/A"),
}
UNASSIGNED = ("200,200,200", "0", "Not yet assigned by VCEP")

AUTOSQL = """table TP53PVS1Splice
"TP53 VCEP PVS1 splice-site variants (+/- 1,2) from CSpec GN009 v2.4.0 Table S1"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "HGVSc variant"
   uint   score;       "Not used, all 0"
   char[1] strand;     "Not used, all ."
   uint   thickStart;  "Same as chromStart"
   uint   thickEnd;    "Same as chromEnd"
   uint   reserved;    "RGB color"
   string acmgCode;    "VCEP-assigned PVS1 strength"
   string points;      "Tavtigian points"
   string assignSource; "'Assigned' or 'Inherited from site anchor'"
   lstring rationale;  "VCEP rationale text (from Table S1)"
   uint   exonNum;     "mRNA exon number"
   lstring _mouseOver; "HTML mouseover"
   )
"""


def cdna_splice_to_genomic(exon_num, sign, offset, tx):
    """Return BED (g_start, g_end) for a canonical +/-1/+/-2 splice variant."""
    n_exons = len(tx['exonStarts'])
    if tx['strand'] == '-':
        idx = n_exons - exon_num
    else:
        idx = exon_num - 1
    ex_g_start = tx['exonStarts'][idx]
    ex_g_end = tx['exonEnds'][idx]
    if tx['strand'] == '-':
        if sign == '+':
            g = ex_g_start - offset
        else:
            g = ex_g_end + offset - 1
    else:
        if sign == '+':
            g = ex_g_end + offset - 1
        else:
            g = ex_g_start - offset
    return (g, g + 1)


def load_variants(src_xlsx):
    wb = openpyxl.load_workbook(src_xlsx, data_only=True)
    ws = wb["Table S1"]
    variants = []
    for row in ws.iter_rows(min_row=4, values_only=True):
        if row[0] is None or not row[2] or not isinstance(row[2], str):
            continue
        m = VAR_RE.match(row[2].strip())
        if not m:
            continue
        variants.append({
            'exon': int(row[0]),
            'variant': row[2].strip(),
            'c_pos': int(m.group(1)),
            'sign': m.group(2),
            'offset': int(m.group(3)),
            'ref': m.group(4),
            'alt': m.group(5),
            'pvs1_raw': row[15],
            'rationale_raw': row[16],
        })
    # Forward-fill within each splice-site position
    by_site = {}
    for v in variants:
        key = (v['c_pos'], v['sign'], v['offset'])
        by_site.setdefault(key, []).append(v)
    for _key, vs in by_site.items():
        anchor_pvs1 = next((x['pvs1_raw'] for x in vs if x['pvs1_raw']), None)
        anchor_rat = next((x['rationale_raw'] for x in vs if x['rationale_raw']), None)
        for x in vs:
            if x['pvs1_raw']:
                x['final_pvs1'] = x['pvs1_raw']
                x['final_rat'] = x['rationale_raw'] or anchor_rat
                x['assign_source'] = "Assigned"
            elif anchor_pvs1:
                x['final_pvs1'] = anchor_pvs1
                x['final_rat'] = anchor_rat
                x['assign_source'] = "Inherited from site anchor"
            else:
                x['final_pvs1'] = None
                x['final_rat'] = None
                x['assign_source'] = "Not yet assigned"
    return variants


def mouseover(v, color, pts, acmg):
    # Build rationale line: assigned variants should not say "not explicitly
    # assigned" just because the rationale cell is blank.
    if v['final_rat']:
        rat_text = v['final_rat']
    elif v['final_pvs1']:
        rat_text = ("VCEP-assigned strength; detailed rationale not populated "
                    "in Table S1 for this position.")
    else:
        rat_text = "VCEP has not explicitly assigned a strength for this splice site."
    return lib.html_ascii_safe((
        "<b>PVS1 splice-site variant:</b> {name} ({pts} pts)"
        "<br><b>ACMG code:</b> {acmg}"
        "<br><b>Exon:</b> {exon} ({sign_label})"
        "<br><b>Assignment:</b> {src}"
        "<br><b>VCEP rationale:</b> {rat}"
        "<br><b>Source:</b> CSpec GN009 v2.4.0 Supplementary Table S1"
    ).format(
        name=v['variant'], pts=pts, acmg=acmg,
        exon=v['exon'],
        sign_label="donor ({}{})".format(v['sign'], v['offset']) if v['sign'] == '+'
        else "acceptor ({}{})".format(v['sign'], v['offset']),
        src=v['assign_source'], rat=rat_text,
    ))


def generate_bed(variants, tx):
    lines = []
    chrom = tx['chrom']
    for v in variants:
        try:
            g_start, g_end = cdna_splice_to_genomic(v['exon'], v['sign'], v['offset'], tx)
        except IndexError:
            continue
        if g_start >= g_end:
            continue
        final = v['final_pvs1']
        if final and final in STRENGTH_MAP:
            color, pts, acmg = STRENGTH_MAP[final]
        else:
            color, pts, acmg = UNASSIGNED
        lines.append("\t".join([
            chrom, str(g_start), str(g_end),
            v['variant'], "0", ".",
            str(g_start), str(g_end),
            color, acmg, pts, v['assign_source'],
            lib.html_ascii_safe(v['final_rat'] or ""),
            str(v['exon']),
            mouseover(v, color, pts, acmg),
        ]))
    return lines


def build(db, outdir, src_xlsx):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    tx = lib.get_transcript_info(db)
    variants = load_variants(src_xlsx)
    print("  {} splice variants parsed".format(len(variants)))
    from collections import Counter
    dist = Counter(v['final_pvs1'] or "Unassigned" for v in variants)
    print("  Strength distribution:")
    for k, n in dist.most_common():
        print("    {}: {}".format(k, n))
    bed_lines = generate_bed(variants, tx)
    print("  {} BED rows".format(len(bed_lines)))

    as_file = os.path.join(outdir, "TP53PVS1Splice.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53PVS1Splice_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.bash("sort -k1,1 -k2,2n {0} -o {0}".format(bed))
    bb = os.path.join(outdir, "TP53PVS1Splice{}.bb".format(db.capitalize()))
    lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+6")
    print("  wrote {}".format(bb))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('-o', '--output-dir', default=DEFAULT_OUTDIR)
    p.add_argument('--db', action='append', help='hg38 or hg19 (repeat). Default hg38.')
    p.add_argument('--src', default=DEFAULT_SRC, help='Source XLSX (Table S1)')
    args = p.parse_args()
    dbs = args.db if args.db else ['hg38']
    for db in dbs:
        build(db, args.output_dir, args.src)


if __name__ == "__main__":
    main()
