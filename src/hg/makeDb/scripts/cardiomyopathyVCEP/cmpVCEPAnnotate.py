#!/usr/bin/env python3
"""
Phase 1 &#8212; hgVai annotation layer for the Cardiomyopathy VCEP hub (RM37446).

Annotates the gnomAD-observed variant universe (the SAME universe used by the
AF + Provisional tracks) with protein consequence via the internal UCSC tool
hgVai (vai.pl), on the single MANE Select transcript per gene (ncbiRefSeqSelect).
Reuses cmpVCEPAFfrequencies.fetch_gene_variants so the universe is identical.

Output (consumed by Phase 2 cmpVCEPProvisionalClass):
  cmpVCEPAnnotate/cmpVCEPAnnotations.hg38.tsv   one row per (chrom,pos,ref,alt)
  cmpVCEPAnnotate/phase1_unmapped.tsv           universe variants with no VEP line
  cmpVCEPAnnotate/<gene>.vcf / <gene>.vep        per-gene intermediates

Annotation is computed on hg38 only; Phase 2 codes carry to hg19 via the existing
liftOver of the universe (annotation is transcript-based, assembly-independent).

Usage:
  python3 cmpVCEPAnnotate.py --output-dir /hive/users/lrnassar/claude/RM37446
"""
import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cmpVCEPAFfrequencies as af

VAI = os.path.expanduser('~/bin/scripts/vai.pl')
HGVAI = '/usr/local/apache/cgi-bin/hgVai'
GENE_TRACK = 'ncbiRefSeqSelect'

# We set the VCF ID column to "chrom:pos:ref:alt"; vai.pl echoes it into the
# "Uploaded Variation" column verbatim, giving an exact join even for indels
# (whose default chrom_pos_ref/alt name VEP reformats).


def write_gene_vcf(variants, path):
    """Write a sorted, deduped minimal VCF for one gene's universe variants."""
    seen = set()
    rows = []
    for v in variants:
        key = (v['chrom'], v['pos'], v['ref'], v['alt'])
        if key in seen:
            continue
        seen.add(key)
        rows.append(key)
    rows.sort(key=lambda k: k[1])
    with open(path, 'w') as fh:
        fh.write('##fileformat=VCFv4.2\n')
        fh.write('#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n')
        for chrom, pos, ref, alt in rows:
            vid = f'{chrom}:{pos}:{ref}:{alt}'
            fh.write(f'{chrom}\t{pos}\t{vid}\t{ref}\t{alt}\t.\t.\t.\n')
    return rows


def run_vai(vcf_path, vep_path, chrom, tx_start, tx_end):
    """Run hgVai over the gene tx region; write VEP output to vep_path."""
    region = f'{chrom}:{tx_start + 1}-{tx_end}'
    cmd = [VAI, '--variantLimit=200000000', f'--hgVai={HGVAI}',
           f'--position={region}', f'--geneTrack={GENE_TRACK}',
           '--hgvsG=off', '--hgvsCN=off', '--hgvsP=on', 'hg38', vcf_path]
    with open(vep_path, 'w') as out:
        subprocess.run(cmd, stdout=out, stderr=subprocess.DEVNULL, check=True)


def parse_extra(extra):
    """Pull HGVSP and EXON out of the VEP Extra column (key=value;key=value)."""
    hgvsp, exon_n, exon_total = '', '', ''
    for kv in extra.split(';'):
        if kv.startswith('HGVSP='):
            hgvsp = kv[len('HGVSP='):]
        elif kv.startswith('EXON='):
            m = re.match(r'(\d+)/(\d+)', kv[len('EXON='):])
            if m:
                exon_n, exon_total = m.group(1), m.group(2)
    return hgvsp, exon_n, exon_total


def parse_vep(vep_path, gene):
    """Parse one gene's VEP output &#8594; dict key (chrom,pos,ref,alt) &#8594; annotation.
    Aggregates SO terms across lines; keeps the coding fields from the line(s)
    that carry them."""
    ann = {}
    for line in open(vep_path):
        if line.startswith('#') or line.startswith('Uploaded'):
            continue
        f = line.rstrip('\n').split('\t')
        if len(f) < 14:
            continue
        parts = f[0].split(':')
        if len(parts) != 4:
            continue
        chrom, pos, ref, alt = parts[0], int(parts[1]), parts[2], parts[3]
        key = (chrom, pos, ref, alt)
        so = f[6]
        cdna_pos = f[7]            # "Position in cDNA" (transcript orientation; for NMD-escape)
        protein_pos = f[9]
        aa_change = f[10]          # "R/H" (missense), "R" (syn), or "-"
        codon = f[11]
        hgvsp, exon_n, exon_total = parse_extra(f[13])

        rec = ann.setdefault(key, {
            'gene': gene, 'so': set(), 'proteinPos': '', 'aaRef': '', 'aaAlt': '',
            'codon': '', 'exonNum': '', 'exonTotal': '', 'hgvsp': '', 'cdnaPos': '',
        })
        rec['so'].add(so)
        # Capture coding fields from whichever line carries them (non-'-').
        if cdna_pos not in ('', '-') and not rec['cdnaPos']:
            rec['cdnaPos'] = cdna_pos.split('-')[0]   # range start for indels
        if protein_pos not in ('', '-') and not rec['proteinPos']:
            rec['proteinPos'] = protein_pos
        if codon not in ('', '-') and not rec['codon']:
            rec['codon'] = codon
        if exon_total and not rec['exonTotal']:
            rec['exonNum'], rec['exonTotal'] = exon_n, exon_total
        if hgvsp and not rec['hgvsp']:
            rec['hgvsp'] = hgvsp
        if aa_change not in ('', '-') and not rec['aaRef']:
            if '/' in aa_change:
                rec['aaRef'], rec['aaAlt'] = aa_change.split('/', 1)
            else:
                rec['aaRef'] = rec['aaAlt'] = aa_change   # synonymous: ref==alt
    return ann


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--output-dir', required=True)
    args = ap.parse_args()

    work = os.path.join(args.output_dir, 'cmpVCEPAnnotate')
    os.makedirs(work, exist_ok=True)

    print('  [Phase 1 hgVai annotation]')
    all_ann = {}
    unmapped = []
    for gene in af.OUR_GENES:
        mane = af.parse_mane_record(gene)
        variants = af.fetch_gene_variants(gene, mane)
        vcf_path = os.path.join(work, f'{gene}.vcf')
        vep_path = os.path.join(work, f'{gene}.vep')
        universe_keys = write_gene_vcf(variants, vcf_path)
        run_vai(vcf_path, vep_path, mane['chrom'], mane['chromStart'], mane['chromEnd'])
        ann = parse_vep(vep_path, gene)
        # Detect universe variants that got no VEP annotation
        ann_keys = set(ann.keys())
        for k in universe_keys:
            if k not in ann_keys:
                unmapped.append((gene,) + k)
        all_ann.update(ann)
        print(f'    {gene} ({mane["chrom"]} {mane["strand"]}): '
              f'{len(universe_keys)} universe, {len(ann)} annotated, '
              f'{sum(1 for k in universe_keys if k not in ann_keys)} unmapped')

    tsv = os.path.join(work, 'cmpVCEPAnnotations.hg38.tsv')
    with open(tsv, 'w') as fh:
        fh.write('#chrom\tpos\tref\talt\tgene\tsoTerms\tproteinPos\taaRef\taaAlt\t'
                 'codonChange\texonNum\texonTotal\thgvsp\tcdnaPos\n')
        for (chrom, pos, ref, alt), r in sorted(all_ann.items(), key=lambda x: (x[0][0], x[0][1])):
            fh.write('\t'.join([
                chrom, str(pos), ref, alt, r['gene'],
                ','.join(sorted(r['so'])), r['proteinPos'], r['aaRef'], r['aaAlt'],
                r['codon'], r['exonNum'], r['exonTotal'], r['hgvsp'], r['cdnaPos'],
            ]) + '\n')
    print(f'  wrote {len(all_ann)} annotations &#8594; {tsv}')

    unmapped_path = os.path.join(work, 'phase1_unmapped.tsv')
    with open(unmapped_path, 'w') as fh:
        fh.write('#gene\tchrom\tpos\tref\talt\n')
        for row in unmapped:
            fh.write('\t'.join(str(x) for x in row) + '\n')
    print(f'  {len(unmapped)} unmapped universe variants logged &#8594; {unmapped_path}')

    # SO-term summary for sanity
    from collections import Counter
    so_counter = Counter()
    for r in all_ann.values():
        for s in r['so']:
            so_counter[s] += 1
    print('  SO-term distribution:')
    for s, c in so_counter.most_common():
        print(f'    {c:6d}  {s}')


if __name__ == '__main__':
    main()
