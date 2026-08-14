#!/usr/bin/env python3
"""Extract popEVE VCF records to a flat TSV for sorting and heatmap conversion.

Usage: zcat grch38_popEVE_ukbb_20250715.vcf.gz | extractPopEve.py > popEve_records.tsv

Reads the combined popEVE GRCh38 VCF on stdin and emits one TSV line per usable
missense record.  Component scores are carried through for the enriched mouseover.
Output columns (tab-separated):
  protein  gene  chrom  pos  wt_aa  prot_pos  var_aa  popEVE
  EVE  ESM1v  popAdjEVE  popAdjESM1v  gapFreq
Records missing a required INFO field, or whose mutant is not a standard
single-residue substitution, are skipped with a (rate-limited) warning.
"""

import sys
import re

STANDARD_AAS = set('ACDEFGHIKLMNPQRSTVWY')   # 20 standard amino acids
RE_MUTANT = re.compile(r'^([A-Z])(\d+)([A-Z])$')


def addChrPrefix(chrom):
    """Convert bare chromosome names to UCSC style (1 -> chr1, MT -> chrM)."""
    if chrom.startswith('chr'):
        return chrom
    if chrom == 'MT':
        return 'chrM'
    return 'chr' + chrom


def parseInfo(infoStr):
    """Parse a VCF INFO field into a dict."""
    d = {}
    for field in infoStr.split(';'):
        if '=' in field:
            k, v = field.split('=', 1)
            d[k] = v
    return d


def main():
    nOut = 0
    nSkip = 0
    warnLeft = 20
    for line in sys.stdin:
        if line.startswith('#'):
            continue
        cols = line.rstrip('\n').split('\t')
        if len(cols) < 8:
            continue
        info = parseInfo(cols[7])

        if not all(k in info for k in ('protein', 'gene', 'mutant', 'popEVE')):
            nSkip += 1
            continue

        # Skip records with no usable popEVE score (e.g. start-codon variants are 'nan').
        try:
            pe = float(info['popEVE'])
        except ValueError:
            nSkip += 1
            continue
        if pe != pe or pe in (float('inf'), float('-inf')):   # NaN / inf
            nSkip += 1
            continue

        m = RE_MUTANT.match(info['mutant'])
        if not m or m.group(1) not in STANDARD_AAS or m.group(3) not in STANDARD_AAS:
            nSkip += 1
            if warnLeft > 0:
                sys.stderr.write("  WARNING: unusable mutant %r in %s\n" %
                                 (info.get('mutant'), info['protein']))
                warnLeft -= 1
            continue

        wtAa, protPos, varAa = m.group(1), m.group(2), m.group(3)
        row = [
            info['protein'],
            info['gene'],
            addChrPrefix(cols[0]),
            cols[1],
            wtAa,
            protPos,
            varAa,
            info['popEVE'],
            info.get('EVE', ''),
            info.get('ESM1v', ''),
            info.get('pop-adjusted_EVE', ''),
            info.get('pop-adjusted_ESM1v', ''),
            info.get('gap_frequency', ''),
        ]
        sys.stdout.write('\t'.join(row) + '\n')
        nOut += 1

    sys.stderr.write("Done: %d records written, %d skipped.\n" % (nOut, nSkip))


if __name__ == '__main__':
    main()
