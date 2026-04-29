#!/usr/bin/env python3
"""
TP53 VCEP Bioinformatic single-amino-acid in-frame deletion track.

Subtrack of the Bioinformatic Predictions composite. Provides PP3/BP4
codes for 408 single-aa in-frame deletions across TP53 from the VCEP's
single_aa_bayesdel.xlsx (VICTOR.ann.del sheet).

Source spreadsheet has BayesDel_nsfp33a_noAF (no Align-GVGD; deletions
can't be scored by alignment-based tools). We apply the standard
ClinGen-calibrated BayesDel cutoffs for PP3/BP4:
  PP3_Moderate (+2):  BayesDel >=  0.27
  PP3          (+1):  0.16 <= BayesDel < 0.27
  BP4          (-1): -0.36 <  BayesDel <= -0.18
  BP4_Moderate (-2):  BayesDel <= -0.36

bigBed 9+5 with filterValues on acmgCode.
"""

import argparse
import os
import re
import sys

import openpyxl

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/bioinformaticDel"
DEFAULT_SRC = "/hive/users/lrnassar/claude/RM37399/tp53_downloads/single_aa_bayesdel.xlsx"

# Func_Detail format example: NM_000546:c.1174_1176delTCA:p.S392del
FUNC_DETAIL_RE = re.compile(
    r'NM_000546(?:\.\d+)?:(c\.[^:]+):(p\.[A-Z][a-z]{0,2}\d+(?:_[A-Z][a-z]{0,2}\d+)?del)'
)

CLASSES = {
    'PP3_Moderate': ("138,111,158", "+2"),
    'PP3':          ("196,181,209", "+1"),
    'No evidence':  ("200,200,200", "0"),
    'BP4':          ("213,247,213", "-1"),
    'BP4_Moderate': ("158,213,200", "-2"),
}

AUTOSQL = """table TP53BioinformaticDel
"TP53 VCEP PP3/BP4 codes for single-aa in-frame deletions (BayesDel-only thresholds)"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "Variant (HGVSp)"
   uint   score;       "Not used, all 0"
   char[1] strand;     "Not used, all ."
   uint   thickStart;  "Same as chromStart"
   uint   thickEnd;    "Same as chromEnd"
   uint   reserved;    "RGB color"
   string acmgCode;    "PP3_Moderate / PP3 / BP4_Moderate / BP4 / No evidence"
   string points;      "Tavtigian points"
   string hgvsc;       "HGVSc cDNA notation"
   string bayesDel;    "BayesDel_nsfp33a_noAF score"
   string funcType;    "Predicted functional type (InFrame, StopGain, etc.)"
   lstring _mouseOver; "HTML mouseover"
   )
"""


def classify_bayesdel(bd):
    if bd is None:
        return 'No evidence'
    if bd >= 0.27:
        return 'PP3_Moderate'
    if bd >= 0.16:
        return 'PP3'
    if bd <= -0.36:
        return 'BP4_Moderate'
    if bd <= -0.18:
        return 'BP4'
    return 'No evidence'


def mouseover(hgvsc, hgvsp, code, points, bayesdel, func_type):
    return (
        "<b>VCEP Bioinformatic &mdash; single-aa in-frame deletion</b>"
        "<br><b>Variant:</b> {hgvsc} ({hgvsp})"
        "<br><b>ACMG code:</b> {code} ({pts} pts)"
        "<br><b>BayesDel (no AF):</b> {bd}"
        "<br><b>Func type:</b> {ftype}"
        "<br><b>Cutoffs:</b> PP3_Mod &ge; 0.27; PP3 &ge; 0.16; "
        "BP4 &le; &minus;0.18; BP4_Mod &le; &minus;0.36"
        "<br><b>Source:</b> CSpec GN009 v2.4.0 single-aa deletion BayesDel calibration"
    ).format(hgvsc=hgvsc, hgvsp=hgvsp, code=code, pts=points,
             bd="{:.4f}".format(bayesdel) if bayesdel is not None else "N/A",
             ftype=func_type or "N/A")


def load_variants(src_xlsx):
    wb = openpyxl.load_workbook(src_xlsx, data_only=True)
    ws = wb["VICTOR.ann.del"]
    out = []
    for row in ws.iter_rows(min_row=2, values_only=True):
        if not row or row[0] is None:
            continue
        pos = row[1]
        ident = row[2] or ""
        ref = row[3] or ""
        alt = row[4] or ""
        bayesdel = row[7]
        func_type = str(row[9] or "")
        func_detail = row[10] or ""
        # Restrict to single-aa in-frame deletions (Func_Type begins with "InFrame").
        # Skip large multi-exon dels that come through as StopGain/SpliceCDS with
        # multi-hundred-bp c.notation strings.
        if not func_type.startswith("InFrame"):
            continue
        m = FUNC_DETAIL_RE.search(func_detail)
        if not m:
            continue
        hgvsc = m.group(1)
        hgvsp = m.group(2)
        if len(hgvsc) > 100:
            continue
        out.append({
            'chrom': "chr17",
            'hg19_pos': int(pos),
            'ref': str(ref),
            'alt': str(alt),
            'hgvsc': hgvsc,
            'hgvsp': hgvsp,
            'bayesdel': float(bayesdel) if bayesdel is not None else None,
            'func_type': func_type,
            'ident': str(ident),
        })
    return out


def lift_hg19_to_hg38(records, outdir):
    chain = "/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz"
    if not os.path.exists(chain):
        chain = "/cluster/data/hg19/bed/liftOver/hg19ToHg38.over.chain.gz"
    in_bed = os.path.join(outdir, ".bdel_lift_in.bed")
    out_bed = os.path.join(outdir, ".bdel_lift_out.bed")
    unmapped = os.path.join(outdir, ".bdel_unmapped.bed")
    with open(in_bed, 'w') as f:
        for r in records:
            ref_len = max(1, len(r['ref']))
            f.write("{}\t{}\t{}\t{}\n".format(
                r['chrom'], r['hg19_pos'] - 1, r['hg19_pos'] - 1 + ref_len, r['ident']))
    lib.bash("liftOver {} {} {} {}".format(in_bed, chain, out_bed, unmapped))
    lookup = {}
    with open(out_bed) as f:
        for line in f:
            flds = line.rstrip("\n").split("\t")
            if len(flds) >= 4:
                lookup[flds[3]] = (flds[0], int(flds[1]), int(flds[2]))
    for p in [in_bed, out_bed, unmapped]:
        if os.path.exists(p):
            os.remove(p)
    return lookup


def emit_rows(records, db, hg38_lookup):
    lines = []
    for r in records:
        if db == 'hg19':
            chrom = r['chrom']
            ref_len = max(1, len(r['ref']))
            start = r['hg19_pos'] - 1
            end = start + ref_len
        else:
            if r['ident'] not in hg38_lookup:
                continue
            chrom, start, end = hg38_lookup[r['ident']]
        code = classify_bayesdel(r['bayesdel'])
        color, points = CLASSES[code]
        mo = mouseover(r['hgvsc'], r['hgvsp'], code, points,
                       r['bayesdel'], r['func_type'])
        lines.append("\t".join([
            chrom, str(start), str(end),
            r['hgvsp'], "0", ".",
            str(start), str(end),
            color, code, points,
            r['hgvsc'],
            "{:.4f}".format(r['bayesdel']) if r['bayesdel'] is not None else "N/A",
            r['func_type'],
            mo,
        ]))
    return lines


def build(db, outdir, src_xlsx):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    records = load_variants(src_xlsx)
    print("  {} parsed deletion rows".format(len(records)))
    from collections import Counter
    print("  Func_Type: {}".format(Counter(r['func_type'] for r in records)))
    classes = Counter(classify_bayesdel(r['bayesdel']) for r in records)
    print("  Code distribution: {}".format(dict(classes)))

    if db == 'hg38':
        lookup = lift_hg19_to_hg38(records, outdir)
        print("  liftOver matched {}/{}".format(len(lookup), len(records)))
        lines = emit_rows(records, 'hg38', lookup)
    else:
        lines = emit_rows(records, 'hg19', None)
    print("  {} BED rows".format(len(lines)))

    as_file = os.path.join(outdir, "TP53BioinformaticDel.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53BioinformaticDel_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(lines) + "\n")
    lib.bash("sort -k1,1 -k2,2n {0} -o {0}".format(bed))
    bb = os.path.join(outdir, "TP53BioinformaticDel{}.bb".format(db.capitalize()))
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
