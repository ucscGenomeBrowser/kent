#!/usr/bin/env python3
"""
B.4 - REVEL track builder (PP3/BP4 evidence per CSpec).

Per-missense REVEL scores in the 8 cardiomyopathy genes' CDS regions, applied to
the CSpec calibration thresholds:
  PP3_supporting  if REVEL >= 0.70
  BP4_supporting  if REVEL <= 0.40
The indeterminate band (0.40 < score < 0.70) is DROPPED - per InSiGHT HCI Priors precedent.
This reduces clutter and surfaces only actionable evidence.

Source: /gbdb/hg38/revel/{a,c,g,t}.bw (per-alt-nucleotide bigwigs from REVEL paper)

Outputs:
  cmpVCEPRevel/cmpVCEPRevel.as
  cmpVCEPRevel/cmpVCEPRevelHg{38,19}.bed + .bb
"""

import argparse, os, subprocess, sys

OUR_GENES = ['MYH7', 'MYBPC3', 'TNNT2', 'TNNI3', 'TPM1', 'ACTC1', 'MYL2', 'MYL3']

REVEL_BW = {nt: f'/gbdb/hg38/revel/{nt}.bw' for nt in 'acgt'}

PP3_THRESHOLD = 0.70
BP4_THRESHOLD = 0.40

# Colors: PP3 light-purple, BP4 light-orange (matching InSiGHT HCI Priors palette spirit)
PP3_COLOR = '180,140,210'
BP4_COLOR = '230,170,80'

CHROM_SIZES = {
    'hg38': '/cluster/data/hg38/chrom.sizes',
    'hg19': '/cluster/data/hg19/chrom.sizes',
}

LIFTOVER_HG38_TO_HG19 = '/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz'

AUTOSQL = """table cmpVCEPRevel
"REVEL missense pathogenicity scores - PP3/BP4 thresholded per CSpec"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Position (BED 0-based)"
    uint    chromEnd;       "End (BED half-open; +1 for SNV)"
    string  name;           "Display name (gene + REF>ALT + code)"
    uint    score;          "0"
    char[1] strand;         "Strand"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "PP3 light-purple or BP4 light-orange"
    string  gene;           "Gene symbol"
    char[1] altAllele;      "Alternate nucleotide"
    double  revelScore;     "REVEL score"
    string  acmgCode;       "PP3_Supporting or BP4_Supporting"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""

# Re-use B.1's MANE parsing
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cmpVCEPClinDomains import parse_mane_record, cds_exons

# Standard genetic code + amino-acid 3-letter names, for the mouseover aa change.
# REVEL is per-alt-nucleotide on the genomic forward strand; without the resulting aa change
# a curator on a minus-strand gene reads the wrong allele's score (e.g. reads R870L for R870H).
CODON1 = {
    'TTT':'F','TTC':'F','TTA':'L','TTG':'L','CTT':'L','CTC':'L','CTA':'L','CTG':'L',
    'ATT':'I','ATC':'I','ATA':'I','ATG':'M','GTT':'V','GTC':'V','GTA':'V','GTG':'V',
    'TCT':'S','TCC':'S','TCA':'S','TCG':'S','CCT':'P','CCC':'P','CCA':'P','CCG':'P',
    'ACT':'T','ACC':'T','ACA':'T','ACG':'T','GCT':'A','GCC':'A','GCA':'A','GCG':'A',
    'TAT':'Y','TAC':'Y','TAA':'*','TAG':'*','CAT':'H','CAC':'H','CAA':'Q','CAG':'Q',
    'AAT':'N','AAC':'N','AAA':'K','AAG':'K','GAT':'D','GAC':'D','GAA':'E','GAG':'E',
    'TGT':'C','TGC':'C','TGA':'*','TGG':'W','CGT':'R','CGC':'R','CGA':'R','CGG':'R',
    'AGT':'S','AGC':'S','AGA':'R','AGG':'R','GGT':'G','GGC':'G','GGA':'G','GGG':'G',
}
AA3 = {'A':'Ala','R':'Arg','N':'Asn','D':'Asp','C':'Cys','Q':'Gln','E':'Glu','G':'Gly',
       'H':'His','I':'Ile','L':'Leu','K':'Lys','M':'Met','F':'Phe','P':'Pro','S':'Ser',
       'T':'Thr','W':'Trp','Y':'Tyr','V':'Val','*':'Ter'}
COMP = str.maketrans('ACGTacgt', 'TGCAtgca')
TWOBIT = '/gbdb/hg38/hg38.2bit'


def _fetch_seq(chrom, start, end):
    out = subprocess.check_output(['twoBitToFa', f'{TWOBIT}:{chrom}:{start}-{end}', 'stdout'], text=True)
    return ''.join(out.splitlines()[1:]).upper()


def build_codon_index(mane):
    """Return (cds coding sequence, {genomic 0-based pos -> CDS index}) for this MANE transcript.
    Positions are in transcription order; minus-strand exons are reverse-complemented."""
    chrom, strand = mane['chrom'], mane['strand']
    seq_parts, order = [], []
    exons = cds_exons(mane)   # ascending genomic (start, end), 0-based half-open
    for s, e in (exons if strand == '+' else reversed(exons)):
        seg = _fetch_seq(chrom, s, e)
        if strand == '+':
            seq_parts.append(seg);                 order.extend(range(s, e))
        else:
            seq_parts.append(seg.translate(COMP)[::-1]); order.extend(range(e - 1, s - 1, -1))
    cds = ''.join(seq_parts)
    return cds, {p: i for i, p in enumerate(order)}


def revel_hgvsp(cds, pos2idx, strand, gpos0, alt_fwd):
    """(hgvsp, short) for a forward-strand SNV at genomic 0-based gpos0, or None if not missense."""
    idx = pos2idx.get(gpos0)
    if idx is None:
        return None
    cnum, cp = idx // 3, idx % 3
    ref_codon = cds[cnum * 3: cnum * 3 + 3]
    if len(ref_codon) < 3:
        return None
    coding_alt = alt_fwd if strand == '+' else alt_fwd.translate(COMP)
    alt_codon = ref_codon[:cp] + coding_alt + ref_codon[cp + 1:]
    ra, aa = CODON1.get(ref_codon), CODON1.get(alt_codon)
    if ra is None or aa is None or ra == aa:
        return None
    n = cnum + 1
    return (f'p.{AA3[ra]}{n}{AA3[aa]}', f'{ra}{n}{aa}')


def fetch_revel_bedgraph(chrom, start, end, alt_nt):
    """Return list of (genomic_start, genomic_end, score) for REVEL alt=alt_nt in this region.

    bigWigToBedGraph collapses runs of identical scores at adjacent positions into a
    single multi-bp BED row. REVEL is per-position-per-alt: each position has its own
    REF allele, so the bedGraph compaction is wrong for our purposes. Split any
    multi-bp run into N consecutive 1-bp records before returning.
    (FULL audit P0 #1 fix, 2026-04-28.)
    """
    cmd = ['bigWigToBedGraph',
           f'-chrom={chrom}', f'-start={start}', f'-end={end}',
           REVEL_BW[alt_nt], 'stdout']
    out = subprocess.check_output(cmd, text=True)
    rows = []
    for line in out.splitlines():
        f = line.split('\t')
        if len(f) < 4:
            continue
        s, e, score = int(f[1]), int(f[2]), float(f[3])
        # Split runs into 1-bp records: REVEL is per-position-per-alt.
        for pos in range(s, e):
            rows.append((pos, pos + 1, score))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', action='append', required=True, choices=['hg38', 'hg19'])
    ap.add_argument('--output-dir', required=True)
    args = ap.parse_args()

    out_dir = os.path.join(args.output_dir, 'cmpVCEPRevel')
    os.makedirs(out_dir, exist_ok=True)

    print('  [B.4 REVEL PP3/BP4]')
    print(f'  thresholds: PP3 if REVEL >= {PP3_THRESHOLD}; BP4 if REVEL <= {BP4_THRESHOLD}')

    bed_lines = []
    n_pp3 = 0
    n_bp4 = 0
    n_dropped = 0

    for gene in OUR_GENES:
        mane = parse_mane_record(gene)
        chrom = mane['chrom']
        strand = mane['strand']
        exons = cds_exons(mane)
        cds, pos2idx = build_codon_index(mane)

        for ex_start, ex_end in exons:
            for alt_nt in 'acgt':
                rows = fetch_revel_bedgraph(chrom, ex_start, ex_end, alt_nt)
                for s, e, score in rows:
                    if score == 0:
                        continue  # 0 = not missense / no REVEL score
                    if score >= PP3_THRESHOLD:
                        code = 'PP3_Supporting'
                        color = PP3_COLOR
                        n_pp3 += 1
                    elif score <= BP4_THRESHOLD:
                        code = 'BP4_Supporting'
                        color = BP4_COLOR
                        n_bp4 += 1
                    else:
                        n_dropped += 1
                        continue  # indeterminate band - drop per InSiGHT precedent
                    name = f'{gene}_{alt_nt.upper()}_{score:.3f}_{code[:3]}'
                    hp = revel_hgvsp(cds, pos2idx, strand, s, alt_nt.upper())
                    aa_html = f'<b>{hp[0]}</b> ({hp[1]})<br>' if hp else ''
                    mouseover = (
                        f'<b>REVEL</b> - {code}<br>'
                        f'{gene} {chrom}:{s+1}<br>'
                        + aa_html
                        + f'<b>REVEL score:</b> {score:.3f} '
                        f'(genomic forward-strand alt {alt_nt.upper()})<br>'
                        + f'<b>CSpec threshold:</b> PP3 &#8805; {PP3_THRESHOLD}; BP4 &#8804; {BP4_THRESHOLD}'
                    )
                    bed_lines.append('\t'.join([
                        chrom, str(s), str(e),
                        name, '0', strand,
                        str(s), str(e), color,
                        gene,
                        alt_nt.upper(),
                        f'{score:.3f}',
                        code,
                        mouseover,
                    ]))
        print(f'  {gene}: scanned {len(exons)} CDS exons')

    print(f'  total: PP3={n_pp3}, BP4={n_bp4}, dropped indeterminate={n_dropped}')

    bed_lines.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))

    as_path = os.path.join(out_dir, 'cmpVCEPRevel.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    hg38_bed = os.path.join(out_dir, 'cmpVCEPRevelHg38.bed')
    with open(hg38_bed, 'w') as f:
        for l in bed_lines:
            f.write(l + '\n')
    print(f'  wrote {len(bed_lines)} BED features -> {hg38_bed}')

    if 'hg38' in args.db:
        hg38_bb = os.path.join(out_dir, 'cmpVCEPRevelHg38.bb')
        cmd = ['bedToBigBed', '-tab', '-type=bed9+5', '-as=' + as_path,
               hg38_bed, CHROM_SIZES['hg38'], hg38_bb]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)
        print(f'  hg38 bigBed: {hg38_bb}')

    if 'hg19' in args.db:
        hg19_bed = os.path.join(out_dir, 'cmpVCEPRevelHg19.bed')
        unmapped = hg19_bed + '.unmapped'
        cmd = ['liftOver', '-bedPlus=9', '-tab', hg38_bed, LIFTOVER_HG38_TO_HG19, hg19_bed, unmapped]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)
        if os.path.getsize(unmapped) > 0:
            n_unmapped = sum(1 for line in open(unmapped) if not line.startswith('#'))
            print(f'  WARNING: {n_unmapped} liftOver unmapped: {unmapped}', file=sys.stderr)
        hg19_bb = os.path.join(out_dir, 'cmpVCEPRevelHg19.bb')
        cmd = ['bedToBigBed', '-tab', '-type=bed9+5', '-as=' + as_path,
               hg19_bed, CHROM_SIZES['hg19'], hg19_bb]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)
        print(f'  hg19 bigBed: {hg19_bb}')


if __name__ == '__main__':
    main()
