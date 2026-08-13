#!/usr/bin/env python3
"""
TP53 VCEP Allele Frequencies (BA1/BS1/PM2) track generator.

Pulls gnomAD v4.1 exome variants at the TP53 locus and classifies them
per CSpec GN009 v2.4.0 thresholds:

    BA1            non-founder grpmax AF >= 0.001                   stand-alone B
    BS1            0.0003 <= non-founder grpmax AF < 0.001          -4 pts
    PM2_Supporting AF < 0.00003 global AND grpmax AF < 0.00004      +1 pt

BA1/BS1 use the max AF of a single non-founder continental ancestry group
(AF_grpmax, col 27), per the CSpec's continental-subpopulation FAF rule, when
that group has >=2000 alleles tested (AN_grpmax, col 26). Global AF (col 15)
and faf95 (col 16) are shown in the mouseover. CHIP note (col 29) surfaced too.

Founder-effect ancestry groups (AJ/FIN/AMI/MID/Remaining) are EXCLUDED per
CSpec: if the top group is a founder group we do not apply BA1/BS1, and because
gnomAD v4.1 exposes only the single top group we cannot recover the next-highest
non-founder group in that case (a documented limitation).
"""

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/afFrequencies"
GNOMAD_BB = "/gbdb/hg38/gnomAD/v4.1/exomes/exomes.bb"

# CSpec GN009 v2.4.0 TP53 thresholds
BA1_FAF      = 0.001
BS1_FAF_LOW  = 0.0003
BS1_FAF_HIGH = 0.001
PM2_AF_GLOBAL_MAX = 0.00003
PM2_AF_GRPMAX_MAX = 0.00004

# CSpec GN009 excludes these founder-effect ancestry groups from the
# frequency-based codes; BA1/BS1 use the max AF of a *non-founder* continental
# ancestry group. gnomAD v4.1 exposes only the single top group (AF_grpmax) with
# no per-group filtering AF, so we threshold on AF_grpmax when that group is
# non-founder and >=2000 alleles were tested (a documented approximation of the
# continental-subpopulation FAF the CSpec specifies).
FOUNDER_GROUPS = {'Ashkenazi Jewish', 'Finnish', 'Amish',
                  'Middle Eastern', 'Remaining'}
GRPMAX_MIN_AN = 2000

COLORS = {
    'BA1':             '2,82,66',         # dark teal (stand-alone B)
    'BS1':             '35,159,134',      # teal
    'PM2_Supporting':  '138,111,158',     # purple
}

POINTS = {
    'BA1':             'stand-alone B',
    'BS1':             '-4 pts',
    'PM2_Supporting':  '+1 pt',
}

RULES = {
    'BA1': 'gnomAD v4.1 non-founder ancestry-group AF >= 0.001 (0.1%)',
    'BS1': 'gnomAD v4.1 non-founder ancestry-group AF in [0.0003, 0.001)',
    'PM2_Supporting': 'gnomAD v4.1 AF < 3e-5 global AND grpmax < 4e-5',
}

AUTOSQL = """table TP53AF
"TP53 VCEP ACMG allele frequency classifications from gnomAD v4.1 exomes"
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
   string acmgCode;     "BA1 / BS1 / PM2_Supporting"
   string points;       "Tavtigian points"
   string af;           "Global AF in gnomAD v4.1 exomes"
   string faf;          "Filter allele frequency (faf95)"
   string grpmax_af;    "AF in the grpmax population"
   string grpmax_pop;   "Population with grpmax AF"
   string chipNote;     "gnomAD CHIP annotation (if any)"
   lstring _mouseOver;  "HTML mouseover"
   )
"""


def classify(af_global, faf, af_grpmax, grpmax_pop, an_grpmax):
    # BA1/BS1 per CSpec GN009 use the filtering AF of a single non-founder
    # continental ancestry group. Approximated here by AF_grpmax when the top
    # group is non-founder and >=2000 alleles were tested.
    grpmax_usable = (af_grpmax is not None
                     and grpmax_pop is not None
                     and grpmax_pop not in FOUNDER_GROUPS
                     and an_grpmax is not None and an_grpmax >= GRPMAX_MIN_AN)
    if grpmax_usable and af_grpmax >= BA1_FAF:
        return 'BA1'
    if grpmax_usable and BS1_FAF_LOW <= af_grpmax < BS1_FAF_HIGH:
        return 'BS1'
    # PM2_Supporting: rare globally AND grpmax under limit
    if (af_global is not None and af_global < PM2_AF_GLOBAL_MAX
            and af_grpmax is not None and af_grpmax < PM2_AF_GRPMAX_MAX
            and (af_global > 0 or af_grpmax > 0)):
        return 'PM2_Supporting'
    return None


def safe_float(s):
    if s is None or s == '' or s == 'N/A':
        return None
    try:
        return float(s)
    except ValueError:
        return None


def mouseover(display, code, ref, alt, af_global, faf, af_grpmax, grpmax_pop, chip):
    chip_line = ""
    if chip and chip not in ('N/A', ''):
        chip_line = "<br><b>CHIP note:</b> {}".format(chip)
    return (
        "<b>Variant:</b> {disp} ({ref}&gt;{alt})"
        "<br><b>ACMG code:</b> {code} ({pts})"
        "<br><b>Rule:</b> {rule}"
        "<br><b>Global AF:</b> {af}"
        "<br><b>FAF (faf95):</b> {faf}"
        "<br><b>grpmax AF:</b> {gmax} ({gpop})"
        "{chip}"
        "<br><b>Source:</b> gnomAD v4.1 exomes"
    ).format(
        disp=display, ref=ref, alt=alt,
        code=code, pts=POINTS[code], rule=RULES[code],
        af="{:.2e}".format(af_global) if af_global is not None else "N/A",
        faf="{:.2e}".format(faf) if faf is not None else "N/A",
        gmax="{:.2e}".format(af_grpmax) if af_grpmax is not None else "N/A",
        gpop=grpmax_pop or "N/A",
        chip=chip_line,
    )


def classify_and_build_rows(tx, chrom):
    """Query gnomAD v4.1 exomes on hg38 and emit a list of classified rows
    keyed by an immutable hg38 identifier. The hg38 identifier is used as
    the 'name' field so the hg19 build can look up the same row after liftOver
    and rewrite the display text to reflect hg19 coords."""
    raw_bed = "/tmp/tp53_gnomad_{}.bed".format(os.getpid())
    subprocess.run(
        ["bigBedToBed", GNOMAD_BB,
         "-chrom=" + chrom,
         "-start=" + str(tx['txStart']),
         "-end=" + str(tx['txEnd']),
         raw_bed],
        check=True)
    with open(raw_bed) as f:
        rows = [line.rstrip("\n").split("\t") for line in f]
    os.remove(raw_bed)
    print("  {} variants in TP53 region (hg38)".format(len(rows)))

    # Build rows keyed on the hg38 display id
    classified = []  # list of dicts with all fields; hg38 coords fixed
    all_records = []  # every gnomAD variant, for the PM2 "present in gnomAD" set
    stats = dict(total=len(rows), BA1=0, BS1=0, PM2=0, skipped=0)
    for r in rows:
        c_start = int(r[1])
        c_end = int(r[2])
        ref = r[9]
        alt = r[10]
        all_records.append({'chrom': chrom, 'hg38_start': c_start,
                            'hg38_end': c_end, 'ref': ref, 'alt': alt})
        af_global = safe_float(r[14])
        faf = safe_float(r[15])
        af_grpmax = safe_float(r[26])
        an_grpmax = safe_float(r[25])
        grpmax_pop = r[23] if r[23] not in ('N/A', '') else None
        chip = r[28] if len(r) > 28 else ''
        hg38_name = "{}-{}-{}-{}".format(chrom, c_start + 1, ref, alt)

        code = classify(af_global, faf, af_grpmax, grpmax_pop, an_grpmax)
        if code is None:
            stats['skipped'] += 1
            continue
        stats[code if code in ('BA1', 'BS1') else 'PM2'] += 1
        classified.append({
            'hg38_name': hg38_name,
            'hg38_start': c_start,
            'hg38_end': c_end,
            'chrom': chrom,
            'ref': ref, 'alt': alt,
            'af_global': af_global, 'faf': faf, 'af_grpmax': af_grpmax,
            'grpmax_pop': grpmax_pop, 'chip': chip,
            'code': code,
        })
    print("  classified: BA1={BA1} BS1={BS1} PM2={PM2} skipped={skipped}".format(**stats))
    return classified, all_records


def write_present_set(all_records, db, outdir):
    """Write the set of every gnomAD variant key ('chrom-pos1-ref-alt') present
    at the TP53 locus, in the coordinates of the requested assembly. The
    Provisional track uses this to tell 'absent from gnomAD' (PM2_Supporting
    applies) apart from 'present but not rare enough to be coded' (no PM2). For
    hg19 the hg38 coords are lifted so the keys match the hg19 Provisional build."""
    path = os.path.join(outdir, "TP53AF_present_{}.txt".format(db))
    keys = []
    if db == 'hg38':
        for r in all_records:
            keys.append("{}-{}-{}-{}".format(
                r['chrom'], r['hg38_start'] + 1, r['ref'], r['alt']))
    else:
        chain = "/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz"
        in_bed = os.path.join(outdir, ".present_lift_in.bed")
        out_bed = os.path.join(outdir, ".present_lift_out.bed")
        unmapped = os.path.join(outdir, ".present_unmapped.bed")
        with open(in_bed, 'w') as f:
            for i, r in enumerate(all_records):
                f.write("{}\t{}\t{}\t{}\n".format(
                    r['chrom'], r['hg38_start'], r['hg38_end'], i))
        lib.run_liftOver(in_bed, chain, out_bed, unmapped)
        idxmap = {}
        with open(out_bed) as f:
            for line in f:
                fl = line.rstrip("\n").split("\t")
                if len(fl) >= 4:
                    idxmap[int(fl[3])] = (fl[0], int(fl[1]))
        for i, r in enumerate(all_records):
            if i in idxmap:
                c, s = idxmap[i]
                keys.append("{}-{}-{}-{}".format(c, s + 1, r['ref'], r['alt']))
        for p in (in_bed, out_bed, unmapped):
            if os.path.exists(p):
                os.remove(p)
    with open(path, 'w') as f:
        f.write("\n".join(keys) + "\n")
    print("  wrote gnomAD present-set: {} keys -> {}".format(len(keys), path))


def emit_rows(classified, assembly, coord_lookup=None):
    """Format rows for the given assembly. coord_lookup is a dict keyed on
    hg38_name &#8594; (chrom, 0-based start, end) for hg19 liftOver. For hg38 pass
    None &#8212; rows use their native hg38 coords."""
    lines = []
    for rec in classified:
        if assembly == 'hg38':
            chrom = rec['chrom']
            start = rec['hg38_start']
            end = rec['hg38_end']
        else:
            if rec['hg38_name'] not in coord_lookup:
                continue
            chrom, start, end = coord_lookup[rec['hg38_name']]
        # Use assembly-appropriate display name so hg19 viewers see hg19 pos
        display = "{}-{}-{}-{}".format(chrom, start + 1, rec['ref'], rec['alt'])
        color = COLORS[rec['code']]
        mo = mouseover(display, rec['code'], rec['ref'], rec['alt'],
                       rec['af_global'], rec['faf'], rec['af_grpmax'],
                       rec['grpmax_pop'], rec['chip'])
        lines.append("\t".join([
            chrom, str(start), str(end),
            display, "0", ".",
            str(start), str(end),
            color, rec['code'], POINTS[rec['code']],
            "{:.2e}".format(rec['af_global']) if rec['af_global'] is not None else "N/A",
            "{:.2e}".format(rec['faf']) if rec['faf'] is not None else "N/A",
            "{:.2e}".format(rec['af_grpmax']) if rec['af_grpmax'] is not None else "N/A",
            rec['grpmax_pop'] or "N/A",
            rec['chip'] or "",
            mo,
        ]))
    return lines


def liftover_hg38_to_hg19(classified, outdir):
    """Lift each hg38 coord to hg19, returning dict hg38_name &#8594; (chrom,start,end)."""
    chain = "/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz"
    input_bed = os.path.join(outdir, ".tp53af_lift_in.bed")
    output_bed = os.path.join(outdir, ".tp53af_lift_out.bed")
    unmapped = os.path.join(outdir, ".tp53af_unmapped.bed")
    with open(input_bed, 'w') as f:
        for rec in classified:
            f.write("{}\t{}\t{}\t{}\n".format(
                rec['chrom'], rec['hg38_start'], rec['hg38_end'], rec['hg38_name']))
    lib.run_liftOver(input_bed, chain, output_bed, unmapped)
    lookup = {}
    with open(output_bed) as f:
        for line in f:
            flds = line.rstrip("\n").split("\t")
            if len(flds) >= 4:
                lookup[flds[3]] = (flds[0], int(flds[1]), int(flds[2]))
    for p in [input_bed, output_bed, unmapped]:
        if os.path.exists(p):
            os.remove(p)
    return lookup


def build(db, outdir):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    # We always query gnomAD on hg38 (the source), then lift to hg19 if needed
    tx_hg38 = lib.get_transcript_info('hg38')
    classified, all_records = classify_and_build_rows(tx_hg38, tx_hg38['chrom'])

    as_file = os.path.join(outdir, "TP53AF.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53AF_{}.bed".format(db))
    bb = os.path.join(outdir, "TP53AF{}.bb".format(db.capitalize()))

    if db == 'hg38':
        lines = emit_rows(classified, 'hg38')
        with open(bed, 'w') as f:
            f.write("\n".join(lines) + "\n")
        lib.run_sort_bed(bed)
        lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+8")
        print("  wrote {}".format(bb))
        write_present_set(all_records, db, outdir)
        return

    # hg19 build: liftOver each record and rewrite display name
    lookup = liftover_hg38_to_hg19(classified, outdir)
    dropped = len(classified) - len(lookup)
    if dropped:
        print("  liftOver dropped {} variants".format(dropped))
    lines = emit_rows(classified, 'hg19', coord_lookup=lookup)
    with open(bed, 'w') as f:
        f.write("\n".join(lines) + "\n")
    lib.run_sort_bed(bed)
    lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+8")
    print("  wrote {}".format(bb))
    write_present_set(all_records, db, outdir)
    return




def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('-o', '--output-dir', default=DEFAULT_OUTDIR)
    p.add_argument('--db', action='append', help='hg38 or hg19 (repeat). Default hg38.')
    args = p.parse_args()
    dbs = args.db if args.db else ['hg38']
    for db in dbs:
        build(db, args.output_dir)


if __name__ == "__main__":
    main()
