#!/usr/bin/env python3
"""
B.9 - Walsh gene-level Odds Ratio track (PS4 calibration source).

Rebuilt from WALSH 2017 (Genetics in Medicine, PMID 27532257) Tables S5A (HCM) and S5B (DCM),
the case-control OR + 95% CI by gene x disease x variant class. GN002 PS4 explicitly cites
Walsh 2017 as the preferred case series and defines PS4 strength by the lower bound of the OR's
95% CI:
   STRONG     CI-lower >= 20   (CSpec, explicit)
   MODERATE   CI-lower >= 10   (CSpec, explicit)
   SUPPORTING CI-lower >= 5    (base ACMG PS4: OR > 5, CI excludes 1.0)
   below threshold otherwise
(Earlier versions used Walsh 2019 Table S1 = non-truncating HCM EF across MAF bins, a different
statistic; superseded per CSpec. Whether the VCEP also wants the 2019/EF values is a Phase-7
question.)

Gene-level features: one per (gene x {HCM,DCM} x {All protein-altering, Truncating, Non-truncating}),
spanning the gene CDS (MANE), filterable by gene / cohortDisease / variantClass / ps4Strength.
hg38 built from MANE; hg19 via liftOver.

Outputs:
  cmpVCEPWalshOR/cmpVCEPWalshOR.as
  cmpVCEPWalshOR/cmpVCEPWalshORHg{38,19}.bed + .bb
"""
import argparse, os, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cmpVCEPClinDomains import parse_mane_record

WALSH2017_XLSX = ('/hive/users/lrnassar/claude/RM37446/cmp_downloads/walsh/'
                  'walsh2017_extracted/Supplementary_Tables_resubmit.xlsx')
SHEETS = [('Table S5A', 'HCM'), ('Table S5B', 'DCM')]
OUR_GENES = {'MYH7', 'MYBPC3', 'TNNT2', 'TNNI3', 'TPM1', 'ACTC1', 'MYL2', 'MYL3'}

CHROM_SIZES = {'hg38': '/cluster/data/hg38/chrom.sizes', 'hg19': '/cluster/data/hg19/chrom.sizes'}
LIFTOVER_HG38_TO_HG19 = '/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz'

PS4_COLOR = {'Strong': '210,0,0', 'Moderate': '230,80,80',
             'Supporting': '245,152,152', 'below threshold': '136,136,136'}

AUTOSQL = """table cmpVCEPWalshOR
"Walsh 2017 gene-level case-control Odds Ratios (PMID 27532257) - PS4 calibration source"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Gene CDS start"
    uint    chromEnd;       "Gene CDS end"
    string  name;           "Display name (gene + disease + variant class + PS4 strength)"
    uint    score;          "0"
    char[1] strand;         "Gene strand"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "PS4 strength color"
    string  gene;           "Gene symbol"
    string  cohortDisease;  "Cohort disease (HCM or DCM)"
    string  variantClass;   "Variant class (All protein-altering / Truncating / Non-truncating)"
    string  oddsRatio;      "OR with 95% CI: e.g. 11.7 (10.6-12.9)"
    double  ciLower;        "OR 95% CI lower bound (drives PS4 strength)"
    string  ps4Strength;    "Computed PS4 strength: Strong/Moderate/Supporting/below threshold"
    string  caseCounts;     "Cases with / total"
    string  controlCounts;  "Controls with / total"
    string  fishers;        "Fisher's exact 2-sided p-value"
    string  source;         "Walsh 2017 Table S5A/S5B, PMID 27532257"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""


def ps4_strength(ci_lo):
    if ci_lo >= 20: return 'Strong'
    if ci_lo >= 10: return 'Moderate'
    if ci_lo >= 5:  return 'Supporting'
    return 'below threshold'


def load_walsh2017():
    """Read S5A (HCM) + S5B (DCM): rows (gene, disease, variant class, counts, OR, CI)."""
    import openpyxl
    wb = openpyxl.load_workbook(WALSH2017_XLSX, read_only=True, data_only=True)
    recs = []
    for sheet, disease in SHEETS:
        ws = wb[sheet]
        for r in ws.iter_rows(values_only=True):
            if not r or r[0] not in OUR_GENES:
                continue
            try:
                or_val, ci_lo, ci_hi = float(r[6]), float(r[7]), float(r[8])
            except (TypeError, ValueError):
                continue   # 'Not tested' / n/a rows
            recs.append({
                'gene': r[0], 'disease': disease, 'vclass': str(r[1]).strip(),
                'cases_with': r[2], 'cases_without': r[3],
                'ctrl_with': r[4], 'ctrl_without': r[5],
                'or': or_val, 'ci_lo': ci_lo, 'ci_hi': ci_hi, 'fishers': r[9],
            })
    print(f'  parsed {len(recs)} Walsh 2017 S5A/S5B rows (8 genes x HCM/DCM x variant class)',
          file=sys.stderr)
    return recs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', action='append', required=True, choices=['hg38', 'hg19'])
    ap.add_argument('--output-dir', required=True)
    args = ap.parse_args()

    out_dir = os.path.join(args.output_dir, 'cmpVCEPWalshOR')
    os.makedirs(out_dir, exist_ok=True)
    print('  [B.9 Walsh 2017 gene-level OR - PS4]')

    recs = load_walsh2017()
    mane_cache = {g: parse_mane_record(g) for g in OUR_GENES}

    bed_lines = []
    strength_counts = {}
    for r in recs:
        mane = mane_cache[r['gene']]
        s, e = mane['thickStart'], mane['thickEnd']
        strength = ps4_strength(r['ci_lo'])
        strength_counts[strength] = strength_counts.get(strength, 0) + 1
        color = PS4_COLOR[strength]
        or_str = f'{r["or"]:.1f} ({r["ci_lo"]:.1f}-{r["ci_hi"]:.1f})'
        case_counts = f'{r["cases_with"]}/{(r["cases_with"] or 0) + (r["cases_without"] or 0)}'
        ctrl_counts = f'{r["ctrl_with"]}/{(r["ctrl_with"] or 0) + (r["ctrl_without"] or 0)}'

        mouse = (
            f'<b>PS4 gene-level</b> - Walsh 2017 case-control OR<br>'
            f'<b>{r["gene"]}</b> - {r["disease"]} cohort, {r["vclass"]}<br>'
            f'<b>OR</b> {or_str}<br>'
            f'<b>PS4 strength (CI-lower {r["ci_lo"]:.1f}):</b> {strength}<br>'
            f'<b>Cases (with/total):</b> {case_counts} | <b>Controls:</b> {ctrl_counts}<br>'
            f'<b>Fisher exact p:</b> {r["fishers"]}<br>'
            f'<b>CSpec PS4 thresholds:</b> Strong CI-lo&#8805;20, Moderate &#8805;10, Supporting &#8805;5. '
            f'Walsh 2017 is the CSpec-cited case series.<br>'
            f'<b>Source:</b> Walsh 2017 Table S5{"A" if r["disease"]=="HCM" else "B"}, PMID 27532257'
        )
        name = f'{r["gene"]}_{r["disease"]}_{r["vclass"].split()[0]}_{strength.split()[0]}'
        bed_lines.append('\t'.join([
            mane['chrom'], str(s), str(e), name, '0', mane['strand'],
            str(s), str(e), color, r['gene'], r['disease'], r['vclass'],
            or_str, f'{r["ci_lo"]:.2f}', strength, case_counts, ctrl_counts,
            str(r['fishers']), 'Walsh 2017 Table S5A/S5B, PMID 27532257', mouse,
        ]))

    bed_lines.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))
    print(f'  {len(bed_lines)} features; PS4 strengths: {strength_counts}')

    as_path = os.path.join(out_dir, 'cmpVCEPWalshOR.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    hg38_bed = os.path.join(out_dir, 'cmpVCEPWalshORHg38.bed')
    with open(hg38_bed, 'w') as f:
        f.write('\n'.join(bed_lines) + '\n')

    if 'hg38' in args.db:
        hg38_bb = os.path.join(out_dir, 'cmpVCEPWalshORHg38.bb')
        subprocess.run(['bedToBigBed', '-tab', '-type=bed9+11', '-as=' + as_path,
                        hg38_bed, CHROM_SIZES['hg38'], hg38_bb], check=True)
        print(f'  hg38 bigBed: {hg38_bb}')

    if 'hg19' in args.db:
        hg19_bed = os.path.join(out_dir, 'cmpVCEPWalshORHg19.bed')
        unmapped = hg19_bed + '.unmapped'
        subprocess.run(['liftOver', '-bedPlus=9', '-tab', hg38_bed, LIFTOVER_HG38_TO_HG19,
                        hg19_bed, unmapped], check=True)
        n_un = sum(1 for line in open(unmapped) if not line.startswith('#')) if os.path.getsize(unmapped) else 0
        if n_un:
            print(f'  WARNING: {n_un} unmapped in hg19 liftOver', file=sys.stderr)
        lines = sorted((l.rstrip('\n') for l in open(hg19_bed) if l.strip()),
                       key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))
        with open(hg19_bed, 'w') as f:
            f.write('\n'.join(lines) + '\n')
        hg19_bb = os.path.join(out_dir, 'cmpVCEPWalshORHg19.bb')
        subprocess.run(['bedToBigBed', '-tab', '-type=bed9+11', '-as=' + as_path,
                        hg19_bed, CHROM_SIZES['hg19'], hg19_bb], check=True)
        print(f'  hg19 bigBed: {hg19_bb}')


if __name__ == '__main__':
    main()
