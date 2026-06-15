#!/usr/bin/env python3
"""
Add allele-count summary fields to the SCHEMA VCF.

The SCHEMA release stores per-cohort case/control arrays (ac_case, an_case,
ac_ctrl, an_ctrl; one value per analysis group). This script sums them into:
  AC, AN, AF              - all samples (case + control)
  AC_CASE, AN_CASE, AF_CASE   - schizophrenia cases only
  AC_CTRL, AN_CTRL, AF_CTRL   - controls only

Usage:
  schema_addAcAnAf.py [input.vcf(.gz|.bgz)] [output.vcf]
Defaults: SCHEMA_variant_results.vcf.bgz -> SCHEMA_variant_results_withAF.vcf
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
    """Process VCF, adding total and case/control AC/AN/AF fields."""

    # New INFO header lines to add
    new_headers = [
        '##INFO=<ID=AC,Number=1,Type=Integer,Description="Total allele count (sum of ac_case and ac_ctrl across all groups)">',
        '##INFO=<ID=AN,Number=1,Type=Integer,Description="Total allele number (sum of an_case and an_ctrl across all groups)">',
        '##INFO=<ID=AF,Number=1,Type=Float,Description="Allele frequency (AC/AN)">',
        '##INFO=<ID=AC_CASE,Number=1,Type=Integer,Description="Allele count in schizophrenia cases (sum of ac_case across all groups)">',
        '##INFO=<ID=AN_CASE,Number=1,Type=Integer,Description="Allele number in schizophrenia cases (sum of an_case across all groups)">',
        '##INFO=<ID=AF_CASE,Number=1,Type=Float,Description="Allele frequency in schizophrenia cases (AC_CASE/AN_CASE)">',
        '##INFO=<ID=AC_CTRL,Number=1,Type=Integer,Description="Allele count in controls (sum of ac_ctrl across all groups)">',
        '##INFO=<ID=AN_CTRL,Number=1,Type=Integer,Description="Allele number in controls (sum of an_ctrl across all groups)">',
        '##INFO=<ID=AF_CTRL,Number=1,Type=Float,Description="Allele frequency in controls (AC_CTRL/AN_CTRL)">',
    ]
    managed_ids = ("AC", "AN", "AF", "AC_CASE", "AN_CASE", "AF_CASE",
                   "AC_CTRL", "AN_CTRL", "AF_CTRL")

    opener = gzip.open if infile.endswith('.gz') or infile.endswith('.bgz') else open

    with opener(infile, 'rt') as fin, open(outfile, 'w') as fout:
        for line in fin:
            if line.startswith('##'):
                # Drop any pre-existing copies of the fields we manage so the
                # script is idempotent when re-run on its own output.
                if any(line.startswith(f'##INFO=<ID={i},') for i in managed_ids):
                    continue
                fout.write(line)
            elif line.startswith('#CHROM'):
                # Insert new INFO headers before the column header line
                for h in new_headers:
                    fout.write(h + '\n')
                fout.write(line)
            else:
                # Data line
                parts = line.rstrip('\n').split('\t')
                info = parse_info(parts[7])

                ac_case = sum_values(info['ac_case']) if 'ac_case' in info else 0
                an_case = sum_values(info['an_case']) if 'an_case' in info else 0
                ac_ctrl = sum_values(info['ac_ctrl']) if 'ac_ctrl' in info else 0
                an_ctrl = sum_values(info['an_ctrl']) if 'an_ctrl' in info else 0

                ac = ac_case + ac_ctrl
                an = an_case + an_ctrl
                af = ac / an if an > 0 else 0.0
                af_case = ac_case / an_case if an_case > 0 else 0.0
                af_ctrl = ac_ctrl / an_ctrl if an_ctrl > 0 else 0.0

                # Strip any managed fields already in the INFO column, then
                # re-append the freshly computed values (keeps re-runs clean).
                kept = [it for it in parts[7].split(';')
                        if it.split('=', 1)[0] not in managed_ids]
                new_fields = (
                    f"AC={ac};AN={an};AF={af:.6g}"
                    f";AC_CASE={ac_case};AN_CASE={an_case};AF_CASE={af_case:.6g}"
                    f";AC_CTRL={ac_ctrl};AN_CTRL={an_ctrl};AF_CTRL={af_ctrl:.6g}"
                )
                parts[7] = ";".join(kept) + ";" + new_fields if kept else new_fields

                fout.write('\t'.join(parts) + '\n')

if __name__ == '__main__':
    infile = sys.argv[1] if len(sys.argv) > 1 else 'SCHEMA_variant_results.vcf.bgz'
    outfile = sys.argv[2] if len(sys.argv) > 2 else 'SCHEMA_variant_results_withAF.vcf'

    print(f"Processing {infile} -> {outfile}")
    process_vcf(infile, outfile)
    print("Done.")
