#!/usr/bin/env python3
"""
B.3 &#8212; gnomAD v4.1 Allele Frequencies track builder.

For each variant in the 8 cardiomyopathy gene coding regions (&#177;20 nt splice padding),
parse gnomAD v4.1 exomes for the per-variant FAF95 (filtering allele frequency, 95% CI
lower bound, popmax) and apply per-gene CSpec thresholds:

  BA1            if FAF95 >= 0.001     (all 8 genes)
  BS1            if FAF95 >= 0.0001    (&#8805;0.0002 for MYBPC3 only)
  PM2_supporting if FAF95 <= 0.00004
  no-code        otherwise (still emitted, useful baseline)

Source: /hive/data/outside/gnomAD.4/v4.1/exomes/gnomad.exomes.v4.1.sites.chr{N}.vcf.bgz
Field: fafmax_faf95_max  (max FAF95 across genetic ancestry groups)

Outputs:
  cmpVCEPAFfrequencies/cmpVCEPAFfrequencies.as
  cmpVCEPAFfrequencies/cmpVCEPAFfrequenciesHg{38,19}.bed + .bb
"""

import argparse, os, re, subprocess, sys

OUR_GENES = ['MYH7', 'MYBPC3', 'TNNT2', 'TNNI3', 'TPM1', 'ACTC1', 'MYL2', 'MYL3']

# Gene-specific BS1 threshold (per CSpec &#8212; MYBPC3 outlier)
BS1_THRESHOLDS = {
    'MYBPC3': 0.0002,
}
DEFAULT_BS1 = 0.0001
BA1_THRESHOLD = 0.001
PM2_SUPPORTING_THRESHOLD = 0.00004
SPLICE_PADDING = 20  # nt up/downstream of CDS exons for splice-region inclusion

GNOMAD_VCF_PATTERN = '/hive/data/outside/gnomAD.4/v4.1/exomes/gnomad.exomes.v4.1.sites.{chrom}.vcf.bgz'

TABIX = '/cluster/bin/x86_64/tabix'

# Colors (matching plan)
COLORS = {
    'BA1':            '0,160,0',     # dark green  &#8212; strong benign frequency
    'BS1':            '120,200,120', # light green &#8212; benign frequency
    'PM2_supporting': '250,160,160', # salmon &#8212; rarity (weak pathogenic). Salmon (not the old
                                     # fuchsia) keeps AF distinct; PM1 regions use magenta-rose 230,3,131.
    'no-code':        '180,180,180', # light gray  &#8212; no AF-based code
}

CHROM_SIZES = {
    'hg38': '/cluster/data/hg38/chrom.sizes',
    'hg19': '/cluster/data/hg19/chrom.sizes',
}

LIFTOVER_HG38_TO_HG19 = '/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz'

AUTOSQL = """table cmpVCEPAFfrequencies
"gnomAD v4.1 allele frequencies in cardiomyopathy gene coding regions, with applied ACMG codes"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Position (BED 0-based)"
    uint    chromEnd;       "End"
    string  name;           "Display name (gene + REF>ALT + code)"
    uint    score;          "0"
    char[1] strand;         "Strand"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "Color by applied code"
    string  gene;           "Gene symbol"
    string  refAllele;      "Reference allele"
    string  altAllele;      "Alternate allele"
    double  faf95;          "fafmax_faf95_max from gnomAD v4.1"
    double  af;             "Overall AF"
    string  grpmax;         "Population with max AF"
    string  acmgCode;       "Applied ACMG code: BA1 / BS1 / PM2_supporting / no-code"
    string  thresholdNote;  "BS1 threshold for this gene (0.0002 MYBPC3, else 0.0001)"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""

# Re-use B.1's MANE parsing
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cmpVCEPClinDomains import parse_mane_record, cds_exons


def parse_vcf_info(info_str):
    """Parse VCF INFO field into dict. Returns dict[key] = first-value-as-string."""
    result = {}
    for kv in info_str.split(';'):
        if '=' in kv:
            k, v = kv.split('=', 1)
            result[k] = v
    return result


def fetch_gene_variants(gene, mane):
    """tabix-query gnomAD VCF for gene CDS region (&#177;SPLICE_PADDING). Returns list of variant dicts."""
    chrom = mane['chrom']
    exons = cds_exons(mane)
    # Build region list: each CDS exon &#177; SPLICE_PADDING
    regions = []
    for ex_start, ex_end in exons:
        regions.append((max(0, ex_start - SPLICE_PADDING), ex_end + SPLICE_PADDING))
    # Merge overlapping regions
    regions.sort()
    merged = []
    for s, e in regions:
        if merged and s <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], e))
        else:
            merged.append((s, e))

    vcf = GNOMAD_VCF_PATTERN.format(chrom=chrom)
    variants = []
    for s, e in merged:
        # tabix uses 1-based inclusive; bed is 0-based half-open
        region = f'{chrom}:{s+1}-{e}'
        try:
            out = subprocess.check_output([TABIX, vcf, region],
                                          text=True, stderr=subprocess.DEVNULL)
        except subprocess.CalledProcessError:
            continue
        for line in out.splitlines():
            if line.startswith('#'):
                continue
            f = line.split('\t')
            if len(f) < 8:
                continue
            pos1 = int(f[1])
            ref = f[3]
            alt = f[4]
            filt = f[6]
            info = parse_vcf_info(f[7])
            # Skip non-PASS variants (filtered)
            if filt != 'PASS':
                continue
            # FAF95 popmax
            faf95 = float(info.get('fafmax_faf95_max', '0') or '0')
            af = float(info.get('AF', '0') or '0')
            grpmax = info.get('grpmax', '')
            variants.append({
                'chrom':  chrom,
                'pos':    pos1,
                'ref':    ref,
                'alt':    alt,
                'faf95':  faf95,
                'af':     af,
                'grpmax': grpmax,
            })
    return variants


def apply_code(faf95, gene):
    """Return (acmgCode, color) for the given FAF95 + gene."""
    bs1_thresh = BS1_THRESHOLDS.get(gene, DEFAULT_BS1)
    if faf95 >= BA1_THRESHOLD:
        return 'BA1', COLORS['BA1'], bs1_thresh
    if faf95 >= bs1_thresh:
        return 'BS1', COLORS['BS1'], bs1_thresh
    if faf95 <= PM2_SUPPORTING_THRESHOLD:
        return 'PM2_supporting', COLORS['PM2_supporting'], bs1_thresh
    return 'no-code', COLORS['no-code'], bs1_thresh


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', action='append', required=True, choices=['hg38', 'hg19'])
    ap.add_argument('--output-dir', required=True)
    args = ap.parse_args()

    out_dir = os.path.join(args.output_dir, 'cmpVCEPAFfrequencies')
    os.makedirs(out_dir, exist_ok=True)

    print('  [B.3 gnomAD v4.1 Allele Frequencies]')
    print(f'  thresholds: BA1&#8805;{BA1_THRESHOLD}, BS1&#8805;{DEFAULT_BS1} (or 0.0002 MYBPC3), PM2_sup&#8804;{PM2_SUPPORTING_THRESHOLD}')

    bed_lines = []
    counts = {'BA1': 0, 'BS1': 0, 'PM2_supporting': 0, 'no-code': 0}

    for gene in OUR_GENES:
        mane = parse_mane_record(gene)
        print(f'  fetching {gene} ({mane["chrom"]} {mane["strand"]})...')
        variants = fetch_gene_variants(gene, mane)
        print(f'    {len(variants)} PASS variants in CDS &#177;{SPLICE_PADDING} nt')

        for v in variants:
            code, color, bs1_thresh = apply_code(v['faf95'], gene)
            counts[code] += 1
            start_bed = v['pos'] - 1
            end_bed = v['pos'] - 1 + len(v['ref'])
            mouseover = (
                f'<b>gnomAD v4.1</b> - {code}<br>'
                f'{gene} {v["chrom"]}:{v["pos"]} {v["ref"]}>{v["alt"]}<br>'
                f'<b>FAF95 (popmax):</b> {v["faf95"]:.2e}<br>'
                f'<b>AF (overall):</b> {v["af"]:.2e}<br>'
                f'<b>Popmax group:</b> {v["grpmax"]}<br>'
                f'<b>CSpec thresholds for {gene}:</b> BA1&#8805;{BA1_THRESHOLD}, BS1&#8805;{bs1_thresh}, PM2_sup&#8804;{PM2_SUPPORTING_THRESHOLD}'
            )
            name = f'{gene}_{v["ref"]}>{v["alt"]}_{code[:6]}'
            bed_lines.append('\t'.join([
                v['chrom'], str(start_bed), str(end_bed),
                name, '0', mane['strand'],
                str(start_bed), str(end_bed), color,
                gene,
                v['ref'],
                v['alt'],
                f'{v["faf95"]:.6e}',
                f'{v["af"]:.6e}',
                v['grpmax'],
                code,
                f'BS1&#8805;{bs1_thresh}',
                mouseover,
            ]))

    print(f'  total counts: {counts}')

    bed_lines.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))

    as_path = os.path.join(out_dir, 'cmpVCEPAFfrequencies.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    hg38_bed = os.path.join(out_dir, 'cmpVCEPAFfrequenciesHg38.bed')
    with open(hg38_bed, 'w') as f:
        for l in bed_lines:
            f.write(l + '\n')
    print(f'  wrote {len(bed_lines)} BED features &#8594; {hg38_bed}')

    if 'hg38' in args.db:
        hg38_bb = os.path.join(out_dir, 'cmpVCEPAFfrequenciesHg38.bb')
        cmd = ['bedToBigBed', '-tab', '-type=bed9+9', '-as=' + as_path,
               hg38_bed, CHROM_SIZES['hg38'], hg38_bb]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)
        print(f'  hg38 bigBed: {hg38_bb}')

    if 'hg19' in args.db:
        hg19_bed = os.path.join(out_dir, 'cmpVCEPAFfrequenciesHg19.bed')
        unmapped = hg19_bed + '.unmapped'
        cmd = ['liftOver', '-bedPlus=9', '-tab', hg38_bed, LIFTOVER_HG38_TO_HG19, hg19_bed, unmapped]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)
        if os.path.getsize(unmapped) > 0:
            n_unmapped = sum(1 for line in open(unmapped) if not line.startswith('#'))
            print(f'  WARNING: {n_unmapped} unmapped: {unmapped}', file=sys.stderr)
        hg19_bb = os.path.join(out_dir, 'cmpVCEPAFfrequenciesHg19.bb')
        cmd = ['bedToBigBed', '-tab', '-type=bed9+9', '-as=' + as_path,
               hg19_bed, CHROM_SIZES['hg19'], hg19_bb]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)
        print(f'  hg19 bigBed: {hg19_bb}')


if __name__ == '__main__':
    main()
