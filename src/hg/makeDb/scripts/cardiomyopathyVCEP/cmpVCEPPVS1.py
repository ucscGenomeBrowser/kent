#!/usr/bin/env python3
"""
B.2 - MYBPC3 PVS1 Evidence track builder (MYBPC3-only, the lone PVS1-applicable gene).

Per GN095 (verified at A.1):
  - NMD-escape region: codons 1254+ (50 nt upstream of last exon-exon junction at exon 33:34)
    PVS1 may not apply to LoF variants in this region (they escape NMD)
  - Micro-exons 10, 11, 14: in silico splice predictions less reliable
  - In-frame exons 2-4, 8-11, 14, 20, 22, 24-27: in-frame deletions may not be true LoF
    (these encode functionally important domains but consequences vary)

Outputs (per-assembly bigBed):
  cmpVCEPPVS1/cmpVCEPPVS1.as
  cmpVCEPPVS1/cmpVCEPPVS1Hg{38,19}.bed + .bb

Total features: ~15 (1 NMD-escape region + 14 in-frame/micro exon caveats)
The 3 micro-exons (10, 11, 14) are tagged as in-frame too - single feature per exon
with both caveats listed in the mouseover.
"""

import argparse, os, subprocess, sys

# Re-use B.1's MANE parsing helpers
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cmpVCEPClinDomains import parse_mane_record, cds_exons, aa_to_genomic_segments

# MYBPC3 GN095 PVS1 caveats - hand-transcribed (will be re-verified at D.0)

# In-frame exons (1-based exon numbering per CDS)
IN_FRAME_EXONS = [2, 3, 4, 8, 9, 10, 11, 14, 20, 22, 24, 25, 26, 27]

# Micro-exons (subset of in-frame; splice prediction unreliable)
MICRO_EXONS = {10, 11, 14}

# NMD-escape: codons 1254+ - PVS1 should not be applied here
NMD_ESCAPE_CODON_START = 1254

PVS1_NMD_ESCAPE_COLOR    = '136,136,136'  # gray - "PVS1 not applicable"
PVS1_MICRO_EXON_COLOR    = '255,140,40'   # orange - splice unreliable
PVS1_INFRAME_EXON_COLOR  = '210,80,40'    # red-orange - caveat for in-frame del

CHROM_SIZES = {
    'hg38': '/cluster/data/hg38/chrom.sizes',
    'hg19': '/cluster/data/hg19/chrom.sizes',
}

LIFTOVER_HG38_TO_HG19 = '/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz'

AUTOSQL = """table cmpVCEPPVS1
"MYBPC3 PVS1 evidence caveats per ClinGen Cardiomyopathy CSpec GN095"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position"
    uint    chromEnd;       "End position"
    string  name;           "Display name"
    uint    score;          "Always 0"
    char[1] strand;         "Strand"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "Display color"
    string  caveatType;     "PVS1_NMD_escape | PVS1_microExon | PVS1_inframeExon"
    string  exonInfo;       "Exon number(s) affected"
    string  description;    "Why PVS1 is modified here"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""


def get_cds_exons_with_index(mane):
    """Like cds_exons() but returns list of (exon_num_1based_in_transcript_order, gs, ge)."""
    exons_genomic = cds_exons(mane)  # genomic-order
    if mane['strand'] == '-':
        exons_in_tx_order = list(reversed(exons_genomic))
    else:
        exons_in_tx_order = exons_genomic
    return [(i + 1, gs, ge) for i, (gs, ge) in enumerate(exons_in_tx_order)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', action='append', required=True, choices=['hg38', 'hg19'])
    ap.add_argument('--output-dir', required=True)
    args = ap.parse_args()

    out_dir = os.path.join(args.output_dir, 'cmpVCEPPVS1')
    os.makedirs(out_dir, exist_ok=True)

    print('  [B.2 MYBPC3 PVS1 caveats]')

    mane = parse_mane_record('MYBPC3')
    print(f'  MYBPC3: {mane["chrom"]} {mane["strand"]} | NM_000256.3 expected, got {mane["refSeqAcc"]}')
    if mane['refSeqAcc'] != 'NM_000256.3':
        sys.exit('MANE transcript mismatch for MYBPC3')

    exons_indexed = get_cds_exons_with_index(mane)
    print(f'  MYBPC3 has {len(exons_indexed)} CDS exons (transcript-order)')

    bed_lines = []

    # 1. NMD-escape region: codons 1254+ -> use aa_to_genomic_segments
    cds_total_nt = sum(ee - es for _, es, ee in exons_indexed)
    last_aa = cds_total_nt // 3  # includes stop
    nmd_segments = aa_to_genomic_segments(NMD_ESCAPE_CODON_START, last_aa, mane)
    print(f'  NMD-escape: codons {NMD_ESCAPE_CODON_START}-{last_aa} -> {len(nmd_segments)} genomic segments')
    for seg_start, seg_end, exon_idx in nmd_segments:
        name = f'MYBPC3_PVS1_NMD_escape_codon{NMD_ESCAPE_CODON_START}+_ex{exon_idx}'
        mouseover = (
            f'<b>PVS1 NMD-escape region</b> - Cardiomyopathy VCEP<br>'
            f'<b>MYBPC3</b> codon {NMD_ESCAPE_CODON_START} onwards (50 nt upstream of last exon-exon junction)<br>'
            f'<b>PVS1 may NOT apply</b> &#8212; premature termination codons in this region may escape nonsense-mediated decay '
            f'and not result in haploinsufficiency.<br>'
            f'<b>Source:</b> ClinGen Cardiomyopathy CSpec GN095 v1.0.0; Nagy &amp; Maquat 1998'
        )
        bed_lines.append('\t'.join([
            mane['chrom'], str(seg_start), str(seg_end),
            name, '0', mane['strand'],
            str(seg_start), str(seg_end),
            PVS1_NMD_ESCAPE_COLOR,
            'PVS1_NMD_escape',
            f'exon-{exon_idx}',
            f'PVS1 may not apply: codons {NMD_ESCAPE_CODON_START}+ escape NMD',
            mouseover,
        ]))

    # 2. In-frame exons (with micro-exon overlay): one feature per exon
    for exon_num in IN_FRAME_EXONS:
        # find this exon in transcript-order
        match = [e for e in exons_indexed if e[0] == exon_num]
        if not match:
            print(f'  WARNING: exon {exon_num} not found in MYBPC3 CDS (max {len(exons_indexed)})', file=sys.stderr)
            continue
        _, ex_start, ex_end = match[0]
        is_micro = exon_num in MICRO_EXONS
        if is_micro:
            caveat_type = 'PVS1_microExon_inframe'
            color = PVS1_MICRO_EXON_COLOR
            description = f'Exon {exon_num}: micro-exon (splice prediction unreliable) AND in-frame (deletions may not be LoF)'
            mouseover = (
                f'<b>PVS1 caveat: micro-exon + in-frame</b> - Cardiomyopathy VCEP<br>'
                f'<b>MYBPC3</b> exon {exon_num}<br>'
                f'<b>Splice predictions less reliable</b> for variants affecting micro-exon splice sites '
                f'(Frank-Hansen et al. 2008).<br>'
                f'<b>In-frame deletions</b> may not result in true loss-of-function &#8212; adjust PVS1 strength accordingly.<br>'
                f'<b>Source:</b> ClinGen Cardiomyopathy CSpec GN095'
            )
        else:
            caveat_type = 'PVS1_inframeExon'
            color = PVS1_INFRAME_EXON_COLOR
            description = f'Exon {exon_num}: in-frame (deletions may not be LoF)'
            mouseover = (
                f'<b>PVS1 caveat: in-frame exon</b> - Cardiomyopathy VCEP<br>'
                f'<b>MYBPC3</b> exon {exon_num}<br>'
                f'<b>In-frame deletions</b> here may not result in true loss-of-function. '
                f'Most encode domains with critical roles in protein function (Carrier et al. 2015), '
                f'but consequences of in-frame deletions vary &#8212; adjust PVS1 strength accordingly.<br>'
                f'<b>Source:</b> ClinGen Cardiomyopathy CSpec GN095'
            )
        name = f'MYBPC3_PVS1_{caveat_type}_ex{exon_num}'
        bed_lines.append('\t'.join([
            mane['chrom'], str(ex_start), str(ex_end),
            name, '0', mane['strand'],
            str(ex_start), str(ex_end),
            color,
            caveat_type,
            f'exon-{exon_num}',
            description,
            mouseover,
        ]))

    print(f'  total BED features: {len(bed_lines)}')

    # Write autoSql
    as_path = os.path.join(out_dir, 'cmpVCEPPVS1.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    # hg38 first
    hg38_bed = os.path.join(out_dir, 'cmpVCEPPVS1Hg38.bed')
    bed_lines.sort(key=lambda l: int(l.split('\t')[1]))
    with open(hg38_bed, 'w') as f:
        for l in bed_lines:
            f.write(l + '\n')

    if 'hg38' in args.db:
        hg38_bb = os.path.join(out_dir, 'cmpVCEPPVS1Hg38.bb')
        cmd = ['bedToBigBed', '-tab', '-type=bed9+4', '-as=' + as_path,
               hg38_bed, CHROM_SIZES['hg38'], hg38_bb]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)
        print(f'  hg38 bigBed: {hg38_bb}')

    if 'hg19' in args.db:
        hg19_bed = os.path.join(out_dir, 'cmpVCEPPVS1Hg19.bed')
        unmapped = hg19_bed + '.unmapped'
        cmd = ['liftOver', '-bedPlus=9', '-tab', hg38_bed, LIFTOVER_HG38_TO_HG19, hg19_bed, unmapped]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)
        if os.path.getsize(unmapped) > 0:
            print(f'  WARNING: liftOver unmapped: {unmapped}', file=sys.stderr)
        hg19_bb = os.path.join(out_dir, 'cmpVCEPPVS1Hg19.bb')
        cmd = ['bedToBigBed', '-tab', '-type=bed9+4', '-as=' + as_path,
               hg19_bed, CHROM_SIZES['hg19'], hg19_bb]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)
        print(f'  hg19 bigBed: {hg19_bb}')

    if 'hg38' in args.db and 'hg19' in args.db:
        n38 = sum(1 for _ in open(hg38_bed))
        n19 = sum(1 for _ in open(os.path.join(out_dir, 'cmpVCEPPVS1Hg19.bed')))
        if n38 == n19:
            print(f'  cross-assembly parity OK: {n38} features each')
        else:
            print(f'  WARNING: parity FAILED - hg38={n38} hg19={n19}', file=sys.stderr)


if __name__ == '__main__':
    main()
