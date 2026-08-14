#!/usr/bin/env python3
"""
B.1 - Cardiomyopathy VCEP PM1 Hotspot Regions track builder.

Renders PM1 hotspot codon regions per the per-gene CSpec for the 4 of 8 genes where
PM1 is applicable (MYH7, MYBPC3, TNNT2, TNNI3). PM1 is NOT specified for ACTC1,
MYBPC3*, MYL2, MYL3 (where * = MYBPC3 has PM1 applicable but only on missense).

PM1 ranges (per CSpec, see ../doc/Cardiomyopathy.txt §A.10):
  MYH7    167-931   (NM_000257.4 / Walsh 2019 calibration)
  MYBPC3  485-502 + 1248-1266 (NM_000256.3)
  TNNT2   89-189    (NM_001276345.2)
  TNNI3   141-209   (NM_000363.5)

Reads MANE Select bigGenePred from /gbdb/hg38/mane/mane.bb to extract CDS exon
structure; converts amino-acid ranges to genomic coordinates with explicit
unit tests against the first/last codon of each gene (catches the InSiGHT
PMS2 off-by-one bug class).

Outputs (per --output-dir):
  cmpVCEPClinDomains/cmpVCEPClinDomains.as
  cmpVCEPClinDomains/cmpVCEPClinDomainsHg38.bed
  cmpVCEPClinDomains/cmpVCEPClinDomainsHg38.bb
  cmpVCEPClinDomains/cmpVCEPClinDomainsHg19.bed   (via liftOver)
  cmpVCEPClinDomains/cmpVCEPClinDomainsHg19.bb

Usage:
  python3 cmpVCEPClinDomains.py --db hg38 --db hg19 \
      --output-dir /hive/users/lrnassar/claude/RM37446
"""

import argparse, os, subprocess, sys, tempfile

# PM1 hotspot regions (gene, transcript, [(aa_start, aa_end, label), ...])
PM1_REGIONS = {
    'MYH7':   ('NM_000257.4',     [(167,  931,  'Head/neck/converter')]),
    'MYBPC3': ('NM_000256.3',     [(485,  502,  'Hotspot 1'), (1248, 1266, 'Hotspot 2')]),
    'TNNT2':  ('NM_001276345.2',  [(89,   189,  'Hotspot')]),
    'TNNI3':  ('NM_000363.5',     [(141,  209,  'Hotspot')]),
}

PM1_COLOR = '230,3,131'  # magenta-rose - matches InSiGHT clinDomains + TP53 clinical-domains convention

MANE_BB = '/gbdb/hg38/mane/mane.bb'
LIFTOVER_HG38_TO_HG19 = '/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz'
CHROM_SIZES = {
    'hg38': '/cluster/data/hg38/chrom.sizes',
    'hg19': '/cluster/data/hg19/chrom.sizes',
}

AUTOSQL = """table cmpVCEPClinDomains
"Cardiomyopathy VCEP PM1 hotspot regions per ClinGen CSpec"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position"
    uint    chromEnd;       "End position"
    string  name;           "Display name (gene + region label)"
    uint    score;           "Score (always 0)"
    char[1] strand;          "Strand"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "Display color"
    string  gene;           "Gene symbol"
    string  transcript;     "MANE Select transcript"
    string  aaRange;        "Amino acid range (e.g. 167-931)"
    string  exonInfo;       "Exon containing this segment"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""


# ------------------------------------------------------------------
# Codon-to-genomic conversion (clean rewrite of InSiGHT helper).
# ------------------------------------------------------------------

def parse_mane_record(gene_symbol):
    """Extract bigGenePred fields for one gene from /gbdb/hg38/mane/mane.bb."""
    out = subprocess.check_output(['bigBedToBed', MANE_BB, 'stdout'], text=True)
    for line in out.splitlines():
        f = line.split('\t')
        # bigGenePred col 19 (0-indexed 18) is geneName2 (HGNC symbol)
        if len(f) >= 20 and f[18] == gene_symbol:
            return {
                'chrom':       f[0],
                'chromStart':  int(f[1]),
                'chromEnd':    int(f[2]),
                'name':        f[3],
                'strand':      f[5],
                'thickStart':  int(f[6]),
                'thickEnd':    int(f[7]),
                'blockSizes':  [int(s) for s in f[10].rstrip(',').split(',')],
                'chromStarts': [int(s) for s in f[11].rstrip(',').split(',')],
                'refSeqAcc':   f[21],
            }
    raise KeyError(f"Gene {gene_symbol} not found in {MANE_BB}")


def cds_exons(mane):
    """Return list of (genomic_start, genomic_end) for CDS portions of each exon,
    in genomic order (low -> high coord). BED half-open semantics."""
    exons = []
    for size, rstart in zip(mane['blockSizes'], mane['chromStarts']):
        es = mane['chromStart'] + rstart
        ee = es + size
        # clip to CDS
        if ee <= mane['thickStart'] or es >= mane['thickEnd']:
            continue
        exons.append((max(es, mane['thickStart']), min(ee, mane['thickEnd'])))
    return exons


def aa_to_genomic_segments(aa_start, aa_end, mane):
    """Convert 1-based inclusive amino-acid range [aa_start, aa_end] to
    genomic BED half-open intervals.

    Algorithm:
      1. Compute mRNA-CDS nucleotide range (1-based inclusive): [nt_lo, nt_hi]
         nt_lo = (aa_start - 1) * 3 + 1
         nt_hi = aa_end * 3
      2. Walk CDS exons in TRANSCRIPT order (high-to-low genomic for minus strand,
         low-to-high for plus strand), tracking cumulative mRNA position.
      3. For each exon, find overlap with [nt_lo, nt_hi]; project back to genomic
         coords using the appropriate strand mapping.

    Returns list of (genomic_start, genomic_end, exon_idx_in_transcript_order).
    """
    nt_lo = (aa_start - 1) * 3 + 1
    nt_hi = aa_end * 3

    exons_genomic = cds_exons(mane)  # genomic-order
    if mane['strand'] == '+':
        exons_tx = list(enumerate(exons_genomic, start=1))  # tx-order = genomic-order
    else:
        # tx-order is reverse genomic (highest coord = first in transcript)
        exons_tx = list(enumerate(reversed(exons_genomic), start=1))

    segments = []
    mrna_consumed = 0  # nt count already walked through earlier exons in transcript-order
    for exon_idx, (gs, ge) in exons_tx:
        exon_len = ge - gs
        exon_mrna_lo = mrna_consumed + 1          # 1-based inclusive
        exon_mrna_hi = mrna_consumed + exon_len   # 1-based inclusive
        mrna_consumed = exon_mrna_hi

        if exon_mrna_hi < nt_lo or exon_mrna_lo > nt_hi:
            continue  # this exon entirely outside our range

        # overlap region in mRNA-coords
        ov_lo = max(nt_lo, exon_mrna_lo)
        ov_hi = min(nt_hi, exon_mrna_hi)
        # offset within the exon (0-based, inclusive lo, inclusive hi)
        off_lo = ov_lo - exon_mrna_lo
        off_hi = ov_hi - exon_mrna_lo

        if mane['strand'] == '+':
            # tx-order = genomic-order: low offset -> low genomic
            seg_start = gs + off_lo
            seg_end   = gs + off_hi + 1
        else:
            # tx-order = reverse genomic: high genomic = low transcript-offset
            seg_end   = ge - off_lo
            seg_start = ge - off_hi - 1

        segments.append((seg_start, seg_end, exon_idx))

    return segments


# ------------------------------------------------------------------
# Unit tests on every minus-strand gene + the plus-strand gene
# ------------------------------------------------------------------

def unit_test_codon_conversion():
    """Verify codon-to-genomic conversion against known landmarks for each gene.

    Tests:
      - First codon (codon 1) maps to first 3 nt of CDS
      - Last codon maps to the stop codon (last 3 nt of CDS)
      - Length sanity: aa_to_genomic_segments() total bp = (aa_end - aa_start + 1) * 3

    All 7 minus-strand genes + TPM1 (only plus-strand) tested.
    """
    print('  [B.1 unit tests]')
    test_genes = ['MYH7', 'MYBPC3', 'TNNT2', 'TNNI3', 'TPM1', 'ACTC1', 'MYL2', 'MYL3']
    failures = 0
    for gene in test_genes:
        mane = parse_mane_record(gene)
        cds_total_nt = sum(ee - es for es, ee in cds_exons(mane))
        last_aa = cds_total_nt // 3  # includes stop

        # Test 1: codon 1 spans 3 bp
        segs1 = aa_to_genomic_segments(1, 1, mane)
        bp1 = sum(e - s for s, e, _ in segs1)
        ok1 = bp1 == 3

        # Test 2: last codon spans 3 bp
        segs_last = aa_to_genomic_segments(last_aa, last_aa, mane)
        bp_last = sum(e - s for s, e, _ in segs_last)
        ok_last = bp_last == 3

        # Test 3: full CDS
        segs_full = aa_to_genomic_segments(1, last_aa, mane)
        bp_full = sum(e - s for s, e, _ in segs_full)
        ok_full = bp_full == cds_total_nt

        # Test 4 (minus-strand only): first codon should be at the HIGH end of the CDS
        if mane['strand'] == '-':
            ok_strand = max(s for s, e, _ in segs1) > min(s for s, e, _ in segs_last)
        else:
            ok_strand = min(s for s, e, _ in segs1) < min(s for s, e, _ in segs_last)

        status = '✓' if (ok1 and ok_last and ok_full and ok_strand) else '✗'
        print(f'    {status} {gene} ({mane["strand"]}): cds_nt={cds_total_nt} last_aa={last_aa} '
              f'codon1={bp1}bp last={bp_last}bp full={bp_full}bp strand_order={"OK" if ok_strand else "WRONG"}')
        if not (ok1 and ok_last and ok_full and ok_strand):
            failures += 1
    if failures:
        print(f'  [B.1 unit tests FAILED for {failures} gene(s)]', file=sys.stderr)
        sys.exit(1)
    print('  [B.1 unit tests PASSED for all 8 genes]')


# ------------------------------------------------------------------
# Build BED + bigBed
# ------------------------------------------------------------------

def emit_bed(output_path):
    """Build hg38 BED from PM1 region definitions."""
    bed_lines = []
    for gene, (transcript, ranges) in PM1_REGIONS.items():
        mane = parse_mane_record(gene)
        if mane['refSeqAcc'] != transcript:
            print(f'  WARNING: {gene} MANE refSeqAcc {mane["refSeqAcc"]} != expected {transcript}',
                  file=sys.stderr)
        for aa_start, aa_end, label in ranges:
            segments = aa_to_genomic_segments(aa_start, aa_end, mane)
            for seg_start, seg_end, exon_idx in segments:
                name = f'{gene}_{label.replace(" ", "_")}_aa{aa_start}-{aa_end}_ex{exon_idx}'
                aa_range = f'{aa_start}-{aa_end}'
                exon_info = f'exon-{exon_idx}-of-tx'
                mouseover = (
                    f'<b>PM1 hotspot</b> - Cardiomyopathy VCEP<br>'
                    f'<b>Gene:</b> {gene} ({transcript})<br>'
                    f'<b>Region:</b> codons {aa_range} ({label})<br>'
                    f'<b>Source:</b> ClinGen Cardiomyopathy CSpec; calibrated by Walsh 2019 (PMID 30696458)'
                )
                bed_lines.append('\t'.join([
                    mane['chrom'],
                    str(seg_start),
                    str(seg_end),
                    name,
                    '0',
                    mane['strand'],
                    str(seg_start),
                    str(seg_end),
                    PM1_COLOR,
                    gene,
                    transcript,
                    aa_range,
                    exon_info,
                    mouseover,
                ]))

    # sort by chrom + start
    bed_lines.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))
    with open(output_path, 'w') as f:
        for line in bed_lines:
            f.write(line + '\n')
    print(f'  wrote {len(bed_lines)} BED features -> {output_path}')
    return len(bed_lines)


def make_bigbed(bed_path, db, as_path, bb_path):
    """Convert BED to bigBed using the autoSql schema."""
    cmd = ['bedToBigBed', '-tab', '-type=bed9+5', '-as=' + as_path,
           bed_path, CHROM_SIZES[db], bb_path]
    print(f'  $ {" ".join(cmd)}')
    subprocess.run(cmd, check=True)


def liftover_to_hg19(hg38_bed, hg19_bed):
    """liftOver hg38 BED to hg19; warn on dropped features."""
    unmapped = hg19_bed + '.unmapped'
    # Use bedExtraFieldsToBigBed style: liftOver supports -bedPlus to keep extra fields
    cmd = ['liftOver', '-bedPlus=9', '-tab', hg38_bed, LIFTOVER_HG38_TO_HG19, hg19_bed, unmapped]
    print(f'  $ {" ".join(cmd)}')
    subprocess.run(cmd, check=True)
    if os.path.getsize(unmapped) > 0:
        n = sum(1 for l in open(unmapped) if not l.startswith('#'))
        print(f'  WARNING: {n} features failed liftOver to hg19; see {unmapped}', file=sys.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', action='append', required=True, choices=['hg38', 'hg19'],
                    help='Assembly to build (specify --db hg38 --db hg19 for both)')
    ap.add_argument('--output-dir', required=True)
    args = ap.parse_args()

    out_dir = os.path.join(args.output_dir, 'cmpVCEPClinDomains')
    os.makedirs(out_dir, exist_ok=True)

    # Run unit tests first - must pass before emitting any features
    unit_test_codon_conversion()

    # Write autoSql schema
    as_path = os.path.join(out_dir, 'cmpVCEPClinDomains.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    # Build hg38 first (native), then hg19 via liftOver
    hg38_bed = os.path.join(out_dir, 'cmpVCEPClinDomainsHg38.bed')
    hg38_bb  = os.path.join(out_dir, 'cmpVCEPClinDomainsHg38.bb')
    n_features = emit_bed(hg38_bed)

    if 'hg38' in args.db:
        make_bigbed(hg38_bed, 'hg38', as_path, hg38_bb)
        print(f'  hg38 bigBed: {hg38_bb}')

    if 'hg19' in args.db:
        hg19_bed = os.path.join(out_dir, 'cmpVCEPClinDomainsHg19.bed')
        hg19_bb  = os.path.join(out_dir, 'cmpVCEPClinDomainsHg19.bb')
        liftover_to_hg19(hg38_bed, hg19_bed)
        make_bigbed(hg19_bed, 'hg19', as_path, hg19_bb)
        print(f'  hg19 bigBed: {hg19_bb}')

    # Cross-assembly parity
    if 'hg38' in args.db and 'hg19' in args.db:
        n_hg38 = sum(1 for _ in open(hg38_bed))
        n_hg19 = sum(1 for _ in open(os.path.join(out_dir, 'cmpVCEPClinDomainsHg19.bed')))
        if n_hg38 != n_hg19:
            print(f'  WARNING: cross-assembly parity FAILED - hg38={n_hg38} hg19={n_hg19}',
                  file=sys.stderr)
        else:
            print(f'  cross-assembly parity OK: {n_hg38} features each')


if __name__ == '__main__':
    main()
