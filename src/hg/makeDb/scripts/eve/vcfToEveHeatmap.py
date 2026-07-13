#!/usr/bin/env python3
"""Convert EVE VCF files to heatmap bigBed format (one entry per protein).

Usage: vcfToEveHeatmap.py <vcf_dir> <output_bed>

Each *_HUMAN.vcf in vcf_dir produces one heatmap BED12+ row.
  Columns = protein positions sorted by ascending genomic coordinate.
  Rows    = 20 standard amino acids (A C D E F G H I K L M N P Q R S T V W Y).
  Wildtype cell at each column is left empty.
  Colors  = blue (EVE=0, benign) -> white (EVE=0.5, uncertain) -> red (EVE=1, pathogenic).
  Mouseover includes wildtype, variant, EVE score, and Class25 classification.
"""

import sys
import os
import re
import glob

STANDARD_AAS  = list('AVLIMFYWRHKDESTNQGCP')   # 20 standard AAs, by class, to match MaveDB
COLOR_BOUNDS  = "0,0.5,1"
COLOR_VALUES  = "#2166ac,#f7f7f7,#d6604d"
LEGEND        = "EVE score: 0=benign (blue) 0.5=uncertain (white) 1=pathogenic (red)"

RE_PROT_MUT = re.compile(r'^([A-Z0-9]+)_([A-Z])(\d+)([A-Z])$')


def add_chr_prefix(chrom):
    """Convert bare chromosome names to UCSC style (1 -> chr1, MT -> chrM)."""
    if chrom.startswith('chr'):
        return chrom
    if chrom == 'MT':
        return 'chrM'
    return 'chr' + chrom


def parse_info(info_str):
    """Parse VCF INFO field into a dict."""
    d = {}
    for field in info_str.split(';'):
        if '=' in field:
            k, v = field.split('=', 1)
            d[k] = v
    return d


def process_vcf(vcf_path):
    """Parse one VCF file.

    Returns (protein_name, uniprot_id, strand, data) or None on error.

    data: {prot_pos (int): {
        'chrom':  str,
        'pos':    int (1-based),
        'wt_aa':  str,
        'scores': {var_aa: float},   # EVE score, rounded to 3 dp
        'class25': {var_aa: str},    # Benign/Uncertain/Pathogenic
    }}
    """
    basename     = os.path.basename(vcf_path)
    protein_name = basename.replace('_HUMAN.vcf', '')

    data     = {}
    seen     = set()   # (prot_pos, var_aa) pairs already stored
    uniprot  = None
    strand   = None

    with open(vcf_path, encoding='utf-8', errors='replace') as fh:
        for line in fh:
            if line.startswith('#'):
                continue
            cols = line.rstrip('\n').split('\t')
            if len(cols) < 8:
                continue

            chrom_raw, pos_str = cols[0], cols[1]
            info = parse_info(cols[7])

            if 'ProtMut' not in info or 'EVE' not in info:
                continue

            m = RE_PROT_MUT.match(info['ProtMut'])
            if not m:
                sys.stderr.write(
                    f'  WARNING: unparseable ProtMut {info["ProtMut"]!r} in {basename}\n'
                )
                continue

            uid, wt_aa, pos_digits, var_aa = m.group(1), m.group(2), m.group(3), m.group(4)
            prot_pos = int(pos_digits)

            key = (prot_pos, var_aa)
            if key in seen:
                continue
            seen.add(key)

            if uniprot is None:
                uniprot = uid
            if strand is None:
                strand = '-' if info.get('RevStr', 'False') == 'True' else '+'

            chrom = add_chr_prefix(chrom_raw)
            pos   = int(pos_str)
            eve   = round(float(info['EVE']), 3)
            cls25 = info.get('Class25', '')

            if prot_pos not in data:
                data[prot_pos] = {
                    'chrom':   chrom,
                    'pos':     pos,
                    'wt_aa':   wt_aa,
                    'scores':  {},
                    'class25': {},
                }

            data[prot_pos]['scores'][var_aa]  = eve
            data[prot_pos]['class25'][var_aa] = cls25

    if not data:
        sys.stderr.write(f'  WARNING: no usable data in {basename}\n')
        return None

    return protein_name, uniprot or 'unknown', strand or '+', data


def build_entry(protein_name, uniprot_id, strand, data, out):
    """Write one heatmap BED12+ line for a protein to file object out."""
    # Sort positions by ascending genomic coordinate (left to right on genome).
    # For reverse-strand genes this runs C-term to N-term, matching genomic orientation.
    sorted_prot_pos = sorted(data.keys(), key=lambda p: data[p]['pos'])

    # Verify single chromosome (should always be true for one protein).
    chroms = {data[p]['chrom'] for p in sorted_prot_pos}
    if len(chroms) > 1:
        sys.stderr.write(
            f'  WARNING: {protein_name} spans multiple chromosomes {chroms} - skipping\n'
        )
        return

    chrom = next(iter(chroms))

    # 0-based codon starts (VCF POS is 1-based).
    starts0     = [data[p]['pos'] - 1 for p in sorted_prot_pos]
    chrom_start = starts0[0]
    chrom_end   = starts0[-1] + 3        # include last codon (3 bp)
    n_cols      = len(sorted_prot_pos)

    # BED score = max EVE * 1000, capped at 1000.
    max_eve = max(
        v for p in sorted_prot_pos for v in data[p]['scores'].values()
    )
    bed_score = min(1000, int(max_eve * 1000))

    block_sizes  = ','.join(['3'] * n_cols) + ','
    chrom_starts = ','.join(str(s - chrom_start) for s in starts0) + ','

    row_count = len(STANDARD_AAS)
    labels    = ','.join(STANDARD_AAS)

    # Build score and label arrays in row-major order
    # (all columns for row 0, then row 1, ...).
    score_parts = []
    label_parts = []

    for aa in STANDARD_AAS:
        for prot_pos in sorted_prot_pos:
            wt = data[prot_pos]['wt_aa']
            if aa == wt:
                # Wildtype cell: leave empty so it renders as background.
                score_parts.append('')
                label_parts.append('')
            elif aa in data[prot_pos]['scores']:
                eve = data[prot_pos]['scores'][aa]
                cls = data[prot_pos]['class25'].get(aa, '')
                score_parts.append(f'{eve:.3f}')
                # Mouseover label rendered as HTML; no commas in text.
                lbl = f'{wt}{prot_pos}&rarr;{aa}: EVE {eve:.3f} ({cls})'
                label_parts.append(f'"{lbl}"')
            else:
                score_parts.append('')
                label_parts.append('')

    score_array = ','.join(score_parts)
    label_array = ','.join(label_parts)

    fields = [
        chrom, chrom_start, chrom_end,
        protein_name, bed_score, strand,
        chrom_start, chrom_end, 0,          # thickStart, thickEnd, itemRgb
        n_cols, block_sizes, chrom_starts,
        row_count, labels,
        COLOR_BOUNDS, COLOR_VALUES,
        score_array, label_array,
        LEGEND, uniprot_id,
    ]

    out.write('\t'.join(str(f) for f in fields) + '\n')


def main():
    if len(sys.argv) != 3:
        sys.exit(f'Usage: {sys.argv[0]} <vcf_dir> <output_bed>')

    vcf_dir    = sys.argv[1]
    output_bed = sys.argv[2]

    vcf_files = sorted(glob.glob(os.path.join(vcf_dir, '*_HUMAN.vcf')))
    if not vcf_files:
        sys.exit(f'ERROR: no *_HUMAN.vcf files found in {vcf_dir}')

    sys.stderr.write(f'Processing {len(vcf_files)} VCF files...\n')

    n_ok   = 0
    n_skip = 0

    with open(output_bed, 'w', encoding='utf-8') as out:
        for i, vcf_path in enumerate(vcf_files, 1):
            if i % 200 == 0:
                sys.stderr.write(f'  {i}/{len(vcf_files)}...\n')
            result = process_vcf(vcf_path)
            if result is None:
                n_skip += 1
                continue
            build_entry(*result, out)
            n_ok += 1

    sys.stderr.write(f'Done: {n_ok} proteins written, {n_skip} skipped.\n')


if __name__ == '__main__':
    main()
