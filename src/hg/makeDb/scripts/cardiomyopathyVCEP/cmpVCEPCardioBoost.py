#!/usr/bin/env python3
"""
B.5 &#8212; CardioBoost disease-specific missense predictor track.

CardioBoost (Zhang et al. 2021, Genome Medicine 13:31) is a cardiomyopathy/arrhythmia
disease-specific missense pathogenicity predictor, complementary to REVEL. We render the
precomputed cardiomyopathy-model scores (cm_prediction.RData &#8594; cardioboost_8genes_hg19.tsv,
exported with /usr/bin/Rscript) for our 8 genes.

IMPORTANT: CardioBoost is NOT a CSpec-specified predictor (the CSpec uses REVEL for PP3/BP4).
This track is informational/supplementary &#8212; it does NOT fire an ACMG code. Whether the VCEP
wants it used is an open question (see Phase 7). The displayed CardioBoost class uses the
tool's OWN published thresholds (>=0.9 pathogenic, <=0.1 benign), cited &#8212; not invented here.

Source is hg19 (GRCh37); we build hg19 natively and liftOver hg19->hg38.

Outputs:
  cmpVCEPCardioBoost/cmpVCEPCardioBoost.as
  cmpVCEPCardioBoost/cmpVCEPCardioBoostHg{38,19}.bed + .bb
"""
import os, subprocess, sys

WORKDIR = '/hive/users/lrnassar/claude/RM37446'
TSV = f'{WORKDIR}/cmp_downloads/cardioboost/cardioboost_8genes_hg19.tsv'
CHROM_SIZES = {'hg38': '/cluster/data/hg38/chrom.sizes', 'hg19': '/cluster/data/hg19/chrom.sizes'}
LIFTOVER_HG19_TO_HG38 = '/cluster/data/hg19/bed/liftOver/hg19ToHg38.over.chain.gz'

# CardioBoost's OWN published classification thresholds (Zhang 2021) &#8212; cited, not invented.
CB_PATH = 0.90
CB_BENIGN = 0.10
COLORS = {'Pathogenic': '210,0,0', 'Benign': '0,160,0', 'Indeterminate': '150,150,150'}

AUTOSQL = """table cmpVCEPCardioBoost
"CardioBoost disease-specific missense predictor (Zhang 2021) - informational, NOT a CSpec code"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Position (BED 0-based)"
    uint    chromEnd;       "End (BED half-open; +1 for SNV)"
    string  name;           "Display name (gene + protein change)"
    uint    score;          "0"
    char[1] strand;         "Strand"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "Color by CardioBoost class"
    string  gene;           "Gene symbol"
    char[1] refAllele;      "Reference nucleotide"
    char[1] altAllele;      "Alternate nucleotide"
    string  hgvsc;          "CardioBoost HGVS c. (Ensembl transcript)"
    string  hgvsp;          "CardioBoost HGVS p. (Ensembl protein)"
    double  cbScore;        "CardioBoost pathogenicity probability (0-1)"
    string  cbClass;        "CardioBoost class at published thresholds (>=0.9 P, <=0.1 B)"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""


def cb_class(score):
    if score >= CB_PATH:
        return 'Pathogenic'
    if score <= CB_BENIGN:
        return 'Benign'
    return 'Indeterminate'


def build_hg19_bed():
    rows = []
    with open(TSV) as fh:
        header = fh.readline()
        for line in fh:
            f = line.rstrip('\n').split('\t')
            if len(f) < 8:
                continue
            chrom_num, pos, ref, alt, gene, hgvsc, hgvsp, score_s = f[:8]
            try:
                pos = int(pos); score = float(score_s)
            except ValueError:
                continue
            if len(ref) != 1 or len(alt) != 1:   # CardioBoost is missense SNVs
                continue
            chrom = 'chr' + chrom_num
            start, end = pos - 1, pos
            cls = cb_class(score)
            pshort = hgvsp.split(':')[-1] if ':' in hgvsp else hgvsp
            name = f'{gene}_{pshort}_{ref}{alt}_{score:.2f}'   # ref/alt keeps same-aa variants distinct
            mouse = (f'<b>CardioBoost</b> - disease-specific missense predictor (Zhang 2021)<br>'
                     f'{gene} {chrom}:{pos} {ref}&gt;{alt} <i>{pshort}</i><br>'
                     f'<b>CardioBoost probability:</b> {score:.3f} (class {cls}; tool thresholds &#8805;{CB_PATH} P, &#8804;{CB_BENIGN} B)<br>'
                     f'<i>Informational &#8212; NOT a CSpec-specified predictor; the CSpec uses REVEL for '
                     f'PP3/BP4. Does not fire an ACMG code.</i>')
            rows.append('\t'.join([chrom, str(start), str(end), name, '0', '+',
                                   str(start), str(end), COLORS[cls], gene, ref, alt,
                                   hgvsc, pshort, f'{score:.6f}', cls, mouse]))
    rows.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))
    return rows


def main():
    out_dir = os.path.join(WORKDIR, 'cmpVCEPCardioBoost')
    os.makedirs(out_dir, exist_ok=True)
    print('  [B.5 CardioBoost predictor]')

    as_path = os.path.join(out_dir, 'cmpVCEPCardioBoost.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    hg19_rows = build_hg19_bed()
    hg19_bed = os.path.join(out_dir, 'cmpVCEPCardioBoostHg19.bed')
    with open(hg19_bed, 'w') as f:
        f.write('\n'.join(hg19_rows) + '\n')
    print(f'  hg19 (native): {len(hg19_rows)} variants')

    hg19_bb = os.path.join(out_dir, 'cmpVCEPCardioBoostHg19.bb')
    subprocess.run(['bedToBigBed', '-tab', '-type=bed9+8', '-as=' + as_path,
                    hg19_bed, CHROM_SIZES['hg19'], hg19_bb], check=True)

    # liftOver hg19 -> hg38
    hg38_bed = os.path.join(out_dir, 'cmpVCEPCardioBoostHg38.bed')
    unmapped = hg38_bed + '.unmapped'
    subprocess.run(['liftOver', '-bedPlus=9', '-tab', hg19_bed, LIFTOVER_HG19_TO_HG38,
                    hg38_bed, unmapped], check=True)
    n_un = sum(1 for line in open(unmapped) if not line.startswith('#')) if os.path.getsize(unmapped) else 0
    print(f'  hg38 (liftOver hg19->hg38): {sum(1 for _ in open(hg38_bed))} variants, {n_un} unmapped')

    # re-sort hg38 bed after liftOver
    lines = [l.rstrip('\n') for l in open(hg38_bed)]
    lines.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))
    with open(hg38_bed, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    hg38_bb = os.path.join(out_dir, 'cmpVCEPCardioBoostHg38.bb')
    subprocess.run(['bedToBigBed', '-tab', '-type=bed9+8', '-as=' + as_path,
                    hg38_bed, CHROM_SIZES['hg38'], hg38_bb], check=True)
    print(f'  wrote bigBeds for both assemblies')


if __name__ == '__main__':
    main()
