#!/usr/bin/env python3
"""
Add AC, AN, AF fields to SCHEMA VCF by summing ac_case+ac_ctrl and an_case+an_ctrl.
"""

import gzip
import sys

def parse_info(info_str):
    """Parse INFO field into dict."""
    fields = {}
    for item in info_str.split(';'):
        if '=' in item:
            key, val = item.split('=', 1)
            fields[key] = val
        else:
            fields[item] = True
    return fields

def sum_values(val_str):
    """Sum comma-separated integer values, skipping missing (.)"""
    total = 0
    for v in val_str.split(','):
        if v != '.' and v != '':
            total += int(v)
    return total

def process_vcf(infile, outfile):
    """Process VCF, adding AC, AN, AF fields."""

    # New INFO header lines to add
    new_headers = [
        '##INFO=<ID=AC,Number=1,Type=Integer,Description="Total allele count (sum of ac_case and ac_ctrl across all groups)">',
        '##INFO=<ID=AN,Number=1,Type=Integer,Description="Total allele number (sum of an_case and an_ctrl across all groups)">',
        '##INFO=<ID=AF,Number=1,Type=Float,Description="Allele frequency (AC/AN)">'
    ]

    opener = gzip.open if infile.endswith('.gz') or infile.endswith('.bgz') else open

    with opener(infile, 'rt') as fin, open(outfile, 'w') as fout:
        for line in fin:
            if line.startswith('##'):
                fout.write(line)
            elif line.startswith('#CHROM'):
                # Insert new INFO headers before the column header line
                for h in new_headers:
                    fout.write(h + '\n')
                fout.write(line)
            else:
                # Data line
                parts = line.rstrip('\n').split('\t')
                info_str = parts[7]
                info = parse_info(info_str)

                # Sum ac_case and ac_ctrl
                ac = 0
                if 'ac_case' in info:
                    ac += sum_values(info['ac_case'])
                if 'ac_ctrl' in info:
                    ac += sum_values(info['ac_ctrl'])

                # Sum an_case and an_ctrl
                an = 0
                if 'an_case' in info:
                    an += sum_values(info['an_case'])
                if 'an_ctrl' in info:
                    an += sum_values(info['an_ctrl'])

                # Calculate AF
                af = ac / an if an > 0 else 0.0

                # Append new fields to INFO
                new_info = f"{info_str};AC={ac};AN={an};AF={af:.6g}"
                parts[7] = new_info

                fout.write('\t'.join(parts) + '\n')

if __name__ == '__main__':
    infile = 'SCHEMA_variant_results.vcf.bgz'
    outfile = 'SCHEMA_variant_results_withAF.vcf'

    print(f"Processing {infile} -> {outfile}")
    process_vcf(infile, outfile)
    print("Done.")
