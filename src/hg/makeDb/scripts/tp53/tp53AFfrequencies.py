#!/usr/bin/env python3
"""
TP53 VCEP Allele Frequencies (BA1/BS1/PM2) track generator.

Reads gnomAD v4.1 exome variants at the TP53 locus from the source sites VCF
(via tabix) and classifies them per CSpec GN009 v2.4.0 thresholds:

    BA1            non-founder ancestry-group faf95 >= 0.001        stand-alone B
    BS1            0.0003 <= non-founder ancestry-group faf95 < 0.001  -4 pts
    PM2_Supporting AF < 0.00003 global AND grpmax AF < 0.00004      +1 pt

BA1/BS1 use the CSpec's continental-subpopulation filtering allele frequency:
the maximum faf95 across the non-founder ancestry groups (afr/amr/eas/nfe/sas).
The /gbdb gnomAD bigBed only carries the overall faf95 and the raw grpmax AF, so
we read the source VCF, which has per-group faf95 (faf95_afr, faf95_amr, ...).
faf95 is a Poisson 95% CI lower bound, so it already discounts small groups (the
CSpec's >=2000-allele requirement is subsumed). Founder-effect groups
(AJ/FIN/MID/Remaining) are excluded by simply not including them in the max.
PM2_Supporting uses global AF plus AF_grpmax as a per-ancestry proxy.
"""

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/afFrequencies"
# Source gnomAD v4.1 exomes sites VCF (tabix-indexed, chr17). Read directly
# because the /gbdb bigBed does not carry per-ancestry faf95.
GNOMAD_VCF = "/hive/data/outside/gnomAD.4/v4.1/exomes/gnomad.exomes.v4.1.sites.chr17.vcf.bgz"
TABIX = "/cluster/bin/x86_64/tabix"

# CSpec GN009 v2.4.0 TP53 thresholds
BA1_FAF      = 0.001
BS1_FAF_LOW  = 0.0003
BS1_FAF_HIGH = 0.001
PM2_AF_GLOBAL_MAX = 0.00003
PM2_AF_GRPMAX_MAX = 0.00004

# Non-founder continental ancestry groups whose faf95 counts toward BA1/BS1
# (gnomAD abbrev -> display name). The founder-effect groups the CSpec excludes
# (asj/fin/mid/remaining) are simply left out.
NON_FOUNDER_FAF = {
    'afr': 'African/African American',
    'amr': 'Admixed American',
    'eas': 'East Asian',
    'nfe': 'European (Non-Finnish)',
    'sas': 'South Asian',
}
# For displaying the AF_grpmax population (may be a founder group).
GRPMAX_POP_NAMES = {
    'afr': 'African/African American', 'amr': 'Admixed American',
    'asj': 'Ashkenazi Jewish', 'eas': 'East Asian', 'fin': 'Finnish',
    'mid': 'Middle Eastern', 'nfe': 'European (Non-Finnish)',
    'sas': 'South Asian', 'remaining': 'Remaining',
}

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
    'BA1': 'gnomAD v4.1 non-founder ancestry-group faf95 >= 0.001 (0.1%)',
    'BS1': 'gnomAD v4.1 non-founder ancestry-group faf95 in [0.0003, 0.001)',
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
   string faf;          "Max faf95 across non-founder ancestry groups (BA1/BS1 metric)"
   string grpmax_af;    "AF in the grpmax population"
   string grpmax_pop;   "Population with grpmax AF"
   string fafGroup;     "Non-founder ancestry group with the max faf95"
   lstring _mouseOver;  "HTML mouseover"
   )
"""


def classify(af_global, af_grpmax, faf_ba1bs1):
    # BA1/BS1 per CSpec GN009 use the filtering AF (faf95) of a single
    # non-founder continental ancestry group; faf_ba1bs1 is the max such value.
    if faf_ba1bs1 is not None and faf_ba1bs1 >= BA1_FAF:
        return 'BA1'
    if faf_ba1bs1 is not None and BS1_FAF_LOW <= faf_ba1bs1 < BS1_FAF_HIGH:
        return 'BS1'
    # PM2_Supporting: rare globally AND grpmax under limit
    if (af_global is not None and af_global < PM2_AF_GLOBAL_MAX
            and af_grpmax is not None and af_grpmax < PM2_AF_GRPMAX_MAX
            and (af_global > 0 or af_grpmax > 0)):
        return 'PM2_Supporting'
    return None


def safe_float(s):
    if s is None or s in ('', 'N/A', '.'):
        return None
    try:
        return float(s)
    except ValueError:
        return None


def parse_info(info):
    """Parse a VCF INFO column into a {key: value} dict (flag keys omitted)."""
    d = {}
    for kv in info.split(';'):
        if '=' in kv:
            k, v = kv.split('=', 1)
            d[k] = v
    return d


def nonfounder_max_faf(info):
    """Return (max faf95 across non-founder continental groups, group display
    name) from a parsed INFO dict, per the CSpec's continental-subpopulation
    FAF rule for BA1/BS1. Returns (None, None) if no group has a faf95."""
    best_v = None
    best_g = None
    for abbr, name in NON_FOUNDER_FAF.items():
        v = safe_float(info.get('faf95_' + abbr))
        if v is not None and (best_v is None or v > best_v):
            best_v = v
            best_g = name
    return best_v, best_g


def mouseover(display, code, ref, alt, af_global, faf, faf_group, af_grpmax, grpmax_pop):
    faf_txt = "{:.2e}".format(faf) if faf is not None else "N/A"
    if faf is not None and faf_group:
        faf_txt = "{} ({})".format(faf_txt, faf_group)
    return (
        "<b>Variant:</b> {disp} ({ref}&gt;{alt})"
        "<br><b>ACMG code:</b> {code} ({pts})"
        "<br><b>Rule:</b> {rule}"
        "<br><b>Global AF:</b> {af}"
        "<br><b>Filtering AF (max non-founder group faf95):</b> {faf}"
        "<br><b>grpmax AF:</b> {gmax} ({gpop})"
        "<br><b>Source:</b> gnomAD v4.1 exomes"
    ).format(
        disp=display, ref=ref, alt=alt,
        code=code, pts=POINTS[code], rule=RULES[code],
        af="{:.2e}".format(af_global) if af_global is not None else "N/A",
        faf=faf_txt,
        gmax="{:.2e}".format(af_grpmax) if af_grpmax is not None else "N/A",
        gpop=grpmax_pop or "N/A",
    )


def classify_and_build_rows(tx, chrom):
    """Read the source gnomAD v4.1 exomes VCF on hg38 via tabix and emit a list
    of classified rows keyed by an immutable hg38 identifier. The hg38 id is used
    as the 'name' field so the hg19 build can look up the same row after liftOver
    and rewrite the display text to reflect hg19 coords. Reading the source VCF
    (not the /gbdb bigBed) gives the per-ancestry faf95 that BA1/BS1 require."""
    region = "{}:{}-{}".format(chrom, tx['txStart'] + 1, tx['txEnd'])
    out = subprocess.run([TABIX, GNOMAD_VCF, region],
                         capture_output=True, text=True, check=True).stdout
    vcf_lines = [ln for ln in out.splitlines() if ln and not ln.startswith('#')]
    print("  {} variants in TP53 region (hg38)".format(len(vcf_lines)))

    classified = []   # list of dicts with all fields; hg38 coords fixed
    all_records = []  # every gnomAD variant, for the PM2 "present in gnomAD" set
    stats = dict(total=len(vcf_lines), BA1=0, BS1=0, PM2=0, skipped=0, multi=0,
                 nonpass=0)
    for ln in vcf_lines:
        f = ln.split('\t')
        pos = int(f[1])          # 1-based VCF POS
        ref = f[3]
        alt = f[4]
        if ',' in alt:           # multi-allelic (none expected in v4.1 sites VCF)
            stats['multi'] += 1
            continue
        # Skip non-PASS records (mostly AC0 = observed in nobody after QC, plus
        # AS_VQSR / InbreedingCoeff filter failures). gnomAD frequency use is
        # PASS-only; more importantly a non-PASS variant must not enter the
        # present-set, or it would wrongly count as "present in gnomAD" and block
        # the absent -> PM2_Supporting rule in the Provisional track.
        if f[6] not in ('PASS', '.'):
            stats['nonpass'] += 1
            continue
        info = parse_info(f[7])
        c_start = pos - 1
        c_end = c_start + len(ref)
        all_records.append({'chrom': chrom, 'hg38_start': c_start,
                            'hg38_end': c_end, 'ref': ref, 'alt': alt})
        af_global = safe_float(info.get('AF'))
        af_grpmax = safe_float(info.get('AF_grpmax'))
        grpmax_pop = GRPMAX_POP_NAMES.get(info.get('grpmax'), info.get('grpmax'))
        faf_ba1bs1, faf_group = nonfounder_max_faf(info)
        hg38_name = "{}-{}-{}-{}".format(chrom, pos, ref, alt)

        code = classify(af_global, af_grpmax, faf_ba1bs1)
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
            'af_global': af_global, 'faf': faf_ba1bs1, 'faf_group': faf_group,
            'af_grpmax': af_grpmax, 'grpmax_pop': grpmax_pop,
            'code': code,
        })
    print("  classified: BA1={BA1} BS1={BS1} PM2={PM2} skipped={skipped} "
          "multiallelic_skipped={multi} nonpass_skipped={nonpass}".format(**stats))
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
                       rec['af_global'], rec['faf'], rec['faf_group'],
                       rec['af_grpmax'], rec['grpmax_pop'])
        lines.append("\t".join([
            chrom, str(start), str(end),
            display, "0", ".",
            str(start), str(end),
            color, rec['code'], POINTS[rec['code']],
            "{:.2e}".format(rec['af_global']) if rec['af_global'] is not None else "N/A",
            "{:.2e}".format(rec['faf']) if rec['faf'] is not None else "N/A",
            "{:.2e}".format(rec['af_grpmax']) if rec['af_grpmax'] is not None else "N/A",
            rec['grpmax_pop'] or "N/A",
            rec['faf_group'] or "N/A",
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
