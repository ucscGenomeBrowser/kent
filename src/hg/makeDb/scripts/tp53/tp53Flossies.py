#!/usr/bin/env python3
"""
TP53 VCEP FLOSSIES BS2 track generator.

FLOSSIES (Female-specific aging cohort, https://whi.color.com/) sequences
TP53 in ~4,942 women >70 unaffected by cancer. Per CSpec GN009 v2.4.0,
observation in this cohort is a BS2 evidence source for TP53 (variant
observed in healthy aged controls weakens pathogenic interpretation).

Source: window.table_variants embedded in
https://whi.color.com/gene/ENSG00000141510 (server-rendered HTML).
Coordinates are hg19 / NM_000546.5; we lift to hg38 and reuse the same
genomic position (NM_000546.5 and .6 are coding-identical for TP53).

bigBed 9+8 with filterValues on bs2Applies and consequence.
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/flossies"
DEFAULT_SRC = "/hive/users/lrnassar/claude/RM37399/flossies_tp53.json"

COLORS = {
    'BS2':            '23,124,106',  # dark teal   - BS2 (8+ carriers)
    'BS2_Moderate':   '46,157,138',  # medium teal - BS2_Moderate (4-7)
    'BS2_Supporting': '138,201,189', # light teal  - BS2_Supporting (2-3)
    'Observed':       '201,201,201', # light gray  - coding, <2 carriers (below BS2)
    'Informational':  '180,180,180', # gray        - non-coding observations
}

# Per CSpec GN009 v2.4.0 §BS2: variant observed in a cohort of healthy
# adult women >70 weakens disease-causation evidence. The VCEP weights BS2
# by the number of carriers in the cohort: 2-3 -> BS2_Supporting, 4-7 ->
# BS2_Moderate, 8+ -> BS2. In the FLOSSIES TP53 export the coding
# observations that carry a consequence in CODING_CONSEQUENCES are all
# missense; those with a single carrier are shown but do not meet BS2. All
# other observations (synonymous, labeled "silent" here, plus UTR / deep
# intronic) are flagged "Informational" since BS2 is not applied to them.
CODING_CONSEQUENCES = {
    'missense_variant', 'synonymous_variant', 'splice_donor_variant',
    'splice_acceptor_variant', 'splice_region_variant', 'stop_gained',
    'stop_lost', 'start_lost', 'frameshift_variant',
    'inframe_insertion', 'inframe_deletion',
}

AUTOSQL = """table TP53Flossies
"TP53 VCEP BS2 evidence: observations in the FLOSSIES healthy-women-over-70 cohort"
   (
   string chrom;        "Reference sequence chromosome or scaffold"
   uint   chromStart;   "Start position in chromosome"
   uint   chromEnd;     "End position in chromosome"
   string name;         "Variant display name"
   uint   score;        "Not used, all 0"
   char[1] strand;      "Not used, all ."
   uint   thickStart;   "Same as chromStart"
   uint   thickEnd;     "Same as chromEnd"
   uint   reserved;     "RGB color"
   string bs2Applies;   "BS2 / BS2_Moderate / BS2_Supporting / Observed / Informational"
   string points;       "Tavtigian points (BS2 -4, Moderate -2, Supporting -1, else 0)"
   string consequence;  "Predicted consequence (missense, synonymous, UTR, etc.)"
   string hgvsc;        "HGVSc cDNA notation"
   string hgvsp;        "HGVSp protein notation (if coding)"
   uint   alleleCount;  "Allele count across the FLOSSIES cohort"
   uint   alleleNum;    "Total allele count (cohort size x 2)"
   string popBreakdown; "Per-population AC/AN"
   string genomicRef;   "Genomic reference allele (matches chromStart base on + strand)"
   string genomicAlt;   "Genomic alternate allele"
   lstring _mouseOver;  "HTML mouseover"
   )
"""


def bs2_tier(consequence, carriers):
    """Return (label, points_str, points_int) per CSpec GN009 v2.4.0 BS2 count
    tiers: 2-3 carriers -> BS2_Supporting, 4-7 -> BS2_Moderate, 8+ -> BS2.
    Coding observations with a single carrier are shown but do not meet BS2;
    non-coding observations are Informational only."""
    if consequence not in CODING_CONSEQUENCES:
        return ('Informational', '0 pts', 0)
    if carriers >= 8:
        return ('BS2', '-4 pts', -4)
    if carriers >= 4:
        return ('BS2_Moderate', '-2 pts', -2)
    if carriers >= 2:
        return ('BS2_Supporting', '-1 pt', -1)
    return ('Observed', '0 pts', 0)


def mouseover(disp, hgvsc, hgvsp, conseq, applies, pts, carriers,
              ac, an, pop_acs, pop_ans):
    pop_lines = []
    for pop in pop_acs:
        an_p = pop_ans.get(pop, 0) or 0
        ac_p = pop_acs.get(pop, 0) or 0
        if an_p:
            pop_lines.append("{}: {}/{} ({:.4f})".format(
                pop, ac_p, an_p, ac_p / an_p))
        else:
            pop_lines.append("{}: {}/{}".format(pop, ac_p, an_p))
    af = (ac / an) if an else 0.0
    bs2_section = ""
    if applies in ('BS2', 'BS2_Moderate', 'BS2_Supporting'):
        bs2_section = (
            "<br><b>BS2 applicability:</b> {applies} ({pts}) "
            "&#8212; {carriers} carrier(s) in cohort "
            "(GN009 tiers: 2&#8211;3 Supporting, 4&#8211;7 Moderate, 8+ Strong)"
        ).format(applies=applies, pts=pts, carriers=carriers)
    elif applies == 'Observed':
        bs2_section = (
            "<br><b>BS2 applicability:</b> Not met &#8212; only {carriers} "
            "carrier(s); GN009 requires &#8805;2 carriers for BS2_Supporting"
        ).format(carriers=carriers)
    else:
        bs2_section = (
            "<br><b>BS2 applicability:</b> Informational only "
            "&#8212; non-coding consequence; BS2 is not standardly applied "
            "to TP53 UTR/intronic changes per CSpec GN009"
        )
    hgvsp_line = ""
    if hgvsp:
        hgvsp_line = "<br><b>Protein:</b> {}".format(hgvsp)
    return (
        "<b>FLOSSIES observation (BS2 evidence)</b>"
        "<br><b>Variant:</b> {disp}"
        "<br><b>cDNA:</b> {hgvsc}"
        "{hgvsp_line}"
        "<br><b>Consequence:</b> {cons}"
        "<br><b>Cohort:</b> healthy women &gt;70 yo (FLOSSIES, WHI/UW)"
        "<br><b>Allele count:</b> {ac}/{an} (AF {af:.4f})"
        "<br><b>By population:</b> {pop}"
        "{bs2}"
        "<br><b>Source:</b> FLOSSIES (whi.color.com); "
        "applied per CSpec GN009 v2.4.0 &#167;BS2"
    ).format(
        disp=disp, hgvsc=hgvsc or "N/A", hgvsp_line=hgvsp_line,
        cons=conseq, ac=ac, an=an, af=af,
        pop=" | ".join(pop_lines) if pop_lines else "N/A",
        bs2=bs2_section,
    )


def load_flossies(src_json):
    with open(src_json) as f:
        return json.load(f)


def hg19_to_hg38_lift(records, outdir):
    """Lift FLOSSIES hg19 coords to hg38; return dict keyed on variant_id."""
    chain = "/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz"
    if not os.path.exists(chain):
        chain = "/cluster/data/hg19/bed/liftOver/hg19ToHg38.over.chain.gz"
    in_bed = os.path.join(outdir, ".flossies_lift_in.bed")
    out_bed = os.path.join(outdir, ".flossies_lift_out.bed")
    unmapped = os.path.join(outdir, ".flossies_unmapped.bed")
    with open(in_bed, 'w') as f:
        for v in records:
            pos = int(v['pos'])
            ref = v['ref']
            f.write("chr{}\t{}\t{}\t{}\n".format(
                v['chrom'], pos - 1, pos - 1 + len(ref), v['variant_id']))
    lib.run_liftOver(in_bed, chain, out_bed, unmapped)
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


def emit_rows(records, assembly, hg38_lookup):
    """Build BED9+8 lines for the requested assembly."""
    lines = []
    skipped = 0
    for v in records:
        ref = v['ref']
        alt = v['alt']
        pos_hg19 = int(v['pos'])
        if assembly == 'hg19':
            chrom = "chr{}".format(v['chrom'])
            start = pos_hg19 - 1
            end = start + len(ref)
        else:
            if v['variant_id'] not in hg38_lookup:
                skipped += 1
                continue
            chrom, start, end = hg38_lookup[v['variant_id']]
        conseq = v.get('major_consequence') or 'unknown'
        ac = int(v.get('allele_count') or 0)
        an = int(v.get('allele_num') or 0)
        hom = int(v.get('hom_count') or 0)
        carriers = ac - hom   # individuals: het + hom = allele_count - hom_count
        applies, pts, _ = bs2_tier(conseq, carriers)
        color = COLORS[applies]
        hgvsc = v.get('HGVSc') or ''
        hgvsp = v.get('HGVSp') or ''
        pop_acs = v.get('pop_acs') or {}
        pop_ans = v.get('pop_ans') or {}
        pop_breakdown = "; ".join(
            "{}={}/{}".format(p, pop_acs.get(p, 0), pop_ans.get(p, 0))
            for p in pop_acs
        )
        disp = "{}:{}{}>{}".format(chrom, start + 1, ref, alt)
        name = hgvsp if hgvsp else (hgvsc if hgvsc else disp)
        mo = mouseover(disp, hgvsc, hgvsp, conseq, applies, pts, carriers,
                       ac, an, pop_acs, pop_ans)
        lines.append("\t".join([
            chrom, str(start), str(end),
            name, "0", ".",
            str(start), str(end),
            color, applies, pts,
            conseq, hgvsc, hgvsp,
            str(ac), str(an), pop_breakdown,
            ref, alt,
            mo,
        ]))
    if skipped:
        print("  liftOver dropped {} variants".format(skipped))
    return lines


def build(db, outdir, src_json):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    records = load_flossies(src_json)
    print("  {} FLOSSIES records loaded".format(len(records)))
    from collections import Counter
    consequences = Counter(r.get('major_consequence') for r in records)
    print("  Consequences: {}".format(dict(consequences)))

    if db == 'hg38':
        hg38_lookup = hg19_to_hg38_lift(records, outdir)
        print("  liftOver matched {}/{} variants".format(
            len(hg38_lookup), len(records)))
        lines = emit_rows(records, 'hg38', hg38_lookup)
    else:
        lines = emit_rows(records, 'hg19', None)
    print("  {} BED rows".format(len(lines)))

    tiers = Counter(L.split("\t")[9] for L in lines)
    print("  BS2 tiers: {}".format(dict(tiers)))

    as_file = os.path.join(outdir, "TP53Flossies.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53Flossies_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(lines) + "\n")
    lib.run_sort_bed(bed)
    bb = os.path.join(outdir, "TP53Flossies{}.bb".format(db.capitalize()))
    lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+10")
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
