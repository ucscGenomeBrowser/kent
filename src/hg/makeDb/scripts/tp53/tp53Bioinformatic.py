#!/usr/bin/env python3
"""
TP53 VCEP Bioinformatic Predictions (PP3/BP4) track generator.

Drives colors from CSpec GN009 v2.4.0 Supplementary Table S2 (2,569 missense
variants with pre-computed PP3/BP4 codes using Align-GVGD + BayesDel + SpliceAI).

bigBed 9+6 with filterValues on acmgCode.
"""

import argparse
import os
import re
import sys

import openpyxl

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/bioinformatic"
DEFAULT_SRC = "/hive/users/lrnassar/claude/RM37399/tp53_downloads/bioinformatic_worksheet.xlsx"

HGVSC_RE = re.compile(r'^c\.(\d+)([ACGT])>([ACGT])$')

CLASSES = {
    'PP3_moderate': ("138,111,158", "+2", "PP3_Moderate"),  # purple
    'PP3':          ("196,181,209", "+1", "PP3"),           # light purple
    'BP4_moderate': ("158,213,200", "-2", "BP4_Moderate"),  # light teal
    'BP4':          ("213,247,213", "-1", "BP4"),           # lime-light
    'No evidence':  ("200,200,200", "0",  "No evidence"),   # gray
}

SPLICE_PP3_THRESHOLD = 0.2  # CSpec GN009: SpliceAI >= 0.2 -> PP3 applies

AUTOSQL = """table TP53Bioinformatic
"TP53 VCEP PP3/BP4 bioinformatic predictions from CSpec GN009 v2.4.0 Table S2"
   (
   string chrom;            "Reference sequence chromosome or scaffold"
   uint   chromStart;       "Start position in chromosome"
   uint   chromEnd;         "End position in chromosome"
   string name;             "Variant (HGVSp)"
   uint   score;            "Not used, all 0"
   char[1] strand;          "Not used, all ."
   uint   thickStart;       "Same as chromStart"
   uint   thickEnd;         "Same as chromEnd"
   uint   reserved;         "RGB color"
   string acmgCode;         "PP3_Moderate / PP3 / BP4_Moderate / BP4 / No evidence"
   string points;           "Tavtigian points"
   string hgvsc;            "HGVSc c.NNN>NNN notation"
   string bayesDel;         "BayesDel_nsfp33a_noAF score"
   string aGVGD;            "Align-GVGD class"
   string spliceAI;         "Maximum SpliceAI delta score"
   string splicePP3Flag;    "Flag: SpliceAI >= 0.2 splicing PP3 applies (overrides missense BP4 if present)"
   lstring _mouseOver;      "HTML mouseover"
   )
"""


def _splice_section(spliceai, code):
    """Format the SpliceAI block. If SpliceAI >= 0.2 and the missense call
    is BP4 / BP4_Moderate, surface a prominent warning &#8212; per Megan Frone
    (TP53 VCEP), splicing PP3 applies and supersedes missense BP4. Per CSpec
    GN009: SpliceAI >= 0.2 -> PP3 applies (splicing-flavored)."""
    if spliceai is None:
        return "<br><b>Max SpliceAI:</b> N/A"
    try:
        sa = float(spliceai)
    except (ValueError, TypeError):
        return "<br><b>Max SpliceAI:</b> {}".format(spliceai)
    if sa >= SPLICE_PP3_THRESHOLD and code in ('BP4', 'BP4_Moderate'):
        return (
            "<br><b>Max SpliceAI:</b> {sa:.2f} &#8212; "
            "<span style='color:#c00000;font-weight:bold'>"
            "splicing PP3 applies (supersedes missense BP4); "
            "interpret as potential splice disruption."
            "</span>"
        ).format(sa=sa)
    if sa >= SPLICE_PP3_THRESHOLD:
        return (
            "<br><b>Max SpliceAI:</b> {sa:.2f} &#8212; splicing PP3 applies"
        ).format(sa=sa)
    return "<br><b>Max SpliceAI:</b> {sa:.2f}".format(sa=sa)


def mouseover(hgvsc, hgvsp, code, points, bayesdel, agvgd, spliceai, note):
    extra = ""
    if note:
        extra = "<br><b>Note:</b> {}".format(str(note)[:400])
    return (
        "<b>Preliminary &#8212; VCEP Table S2</b>"
        "<br><b>Variant:</b> {hgvsc} ({hgvsp})"
        "<br><b>ACMG code:</b> {code} ({points} pts)"
        "<br><b>BayesDel (no AF):</b> {bd}"
        "<br><b>Align-GVGD:</b> {agvgd}"
        "{splice}"
        "{extra}"
        "<br><b>Source:</b> CSpec GN009 v2.4.0 &#167;PP3/BP4"
    ).format(hgvsc=hgvsc, hgvsp=hgvsp, code=code, points=points,
             bd=bayesdel if bayesdel is not None else "N/A",
             agvgd=agvgd or "N/A",
             splice=_splice_section(spliceai, code),
             extra=extra)


def load_variants(src_xlsx):
    wb = openpyxl.load_workbook(src_xlsx, data_only=True)
    ws = wb["Supplementary Table S2"]
    out = []
    for row in ws.iter_rows(min_row=4, values_only=True):
        hgvsc = row[0]
        hgvsp = row[1]
        if not hgvsc or not hgvsp:
            continue
        if not isinstance(hgvsc, str):
            continue
        m = HGVSC_RE.match(hgvsc.strip())
        if not m:
            continue
        c_pos = int(m.group(1))
        ref = m.group(2)
        alt = m.group(3)
        out.append({
            'hgvsc': hgvsc.strip(),
            'hgvsp': hgvsp.strip(),
            'c_pos': c_pos,
            'ref': ref,
            'alt': alt,
            'agvgd': row[2],
            'bayesdel': row[3],
            'code': row[4] or 'No evidence',
            'spliceai': row[5],
            'note': row[6],
        })
    return out


def generate_bed(variants, tx):
    lines = []
    chrom = tx['chrom']
    for v in variants:
        g = lib.cdna_coding_to_genomic(v['c_pos'], tx)
        if g is None:
            continue
        code = v['code']
        if code not in CLASSES:
            code = 'No evidence'
        color, points, display_code = CLASSES[code]
        try:
            sa = float(v['spliceai']) if v['spliceai'] is not None else 0.0
        except (ValueError, TypeError):
            sa = 0.0
        splice_pp3 = "Yes" if sa >= SPLICE_PP3_THRESHOLD else "No"
        mo = mouseover(v['hgvsc'], v['hgvsp'], display_code, points,
                       v['bayesdel'], v['agvgd'], v['spliceai'], v['note'])
        name = v['hgvsp']
        lines.append("\t".join([
            chrom, str(g), str(g + 1),
            name, "0", ".",
            str(g), str(g + 1),
            color, display_code, points,
            v['hgvsc'],
            str(v['bayesdel']) if v['bayesdel'] is not None else "N/A",
            str(v['agvgd']) if v['agvgd'] is not None else "N/A",
            str(v['spliceai']) if v['spliceai'] is not None else "N/A",
            splice_pp3,
            mo,
        ]))
    return lines


def build(db, outdir, src_xlsx):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    tx = lib.get_transcript_info(db)
    variants = load_variants(src_xlsx)
    print("  {} parsed missense rows".format(len(variants)))
    from collections import Counter
    print("  Code distribution: {}".format(Counter(v['code'] for v in variants)))
    bed_lines = generate_bed(variants, tx)
    print("  {} BED rows".format(len(bed_lines)))

    as_file = os.path.join(outdir, "TP53Bioinformatic.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53Bioinformatic_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.run_sort_bed(bed)
    bb = os.path.join(outdir, "TP53Bioinformatic{}.bb".format(db.capitalize()))
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
