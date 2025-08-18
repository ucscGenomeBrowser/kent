#!/usr/bin/env python3

"""Convert multi-sample VCF to MAPLE/"diff" format on stdout, using only the first allele value
in each genotype column (i.e. ignoring any diploid or GT attribute stuff)."""

import sys, logging, argparse, gzip, lzma, re
from collections import defaultdict

def parseCommandLine():
    parser = argparse.ArgumentParser()
    parser.add_argument('vcf', help="Multi-sample monoploid VCF")
    args = parser.parse_args()
    return args

def die(msg):
    logging.error(msg)
    sys.exit(255)

def vcfToMaple(vcf):
    got_header = False
    sample_count = 0
    sample_names = []
    sample_muts = []
    # Ref and alt can only be one base long each (substitutions; indels not supported)
    singleBaseRe = re.compile('^[A-Z]$')
    # Alt allele can be '*' if there are no actual alts, only some Ns
    singleBaseAltRe = re.compile('^[A-Z*]$')
    for line in vcf:
        if line.startswith('#CHROM'):
            got_header = True
            col_names = line.split('\t')
            col_names[-1] = col_names[-1].rstrip()
            sample_names = col_names[9:]
            sample_count = len(sample_names)
            sample_muts = [[] for _ in range(sample_count)]
        elif not line.startswith('#'):
            if not got_header:
                die('Missing #CHROM line in VCF input')
            col_values = line.split('\t')
            col_values[-1] = col_values[-1].rstrip()
            pos = int(col_values[1])
            ref = col_values[3]
            if not singleBaseRe.search(ref):
                die(f"Ref value '{ref}' implies deletion -- sorry, not supported (only substitutions or Ns).")
            alts = col_values[4]
            alt_values = alts.split(',')
            for alt in alt_values:
                if not singleBaseAltRe.search(alt):
                    die(f"Alt value '{alt}' implies insertion or complex variant -- sorry, not supported (only substitutions or Ns).")
            genotypes = col_values[9:]
            gt_count = len(genotypes)
            if (gt_count != sample_count):
                die(f"#CHROM header line defined {sample_count} samples, but found line with {gt_count} genotype columns, pos={pos}")
            for ix, gt in enumerate(genotypes):
                # If gt is from a tool that makes diploid calls ('/' or '|') or attributes (':'),
                # ignore everything but the first alt index.
                gt = gt.split('/')[0].split('|')[0].split(':')[0]
                if gt == '.':
                    sample_muts[ix].append(['n', pos])
                else:
                    al_ix = int(gt)
                    if (al_ix > 0):
                        sample_muts[ix].append([alt_values[al_ix-1].lower(), pos])
    for ix, sample_name in enumerate(sample_names):
        print(f">{sample_name}")
        in_N = False
        n_start = 0
        n_len = 0
        for [base, pos] in sample_muts[ix]:
            if in_N:
                if base == 'n' and pos == n_start + n_len:
                    n_len += 1
                else:
                    print(f"n\t{n_start}\t{n_len}")
                    if base == 'n':
                        n_start = pos
                        n_len = 1
                    else:
                        in_N = False
                        print(f"{base}\t{pos}")
            else:
                if base == 'n':
                    in_N = True
                    n_start = pos
                    n_len = 1
                else:
                    print(f"{base}\t{pos}")
        if in_N:
            print(f"n\t{n_start}\t{n_len}")

def main():
    args = parseCommandLine()
    with gzip.open(args.vcf, 'rt') if args.vcf.endswith('.gz') else lzma.open(args.vcf, 'rt') if args.vcf.endswith('.xz') else open(args.vcf, 'r') as vcf:
        vcfToMaple(vcf)

if __name__ == "__main__":
    main()
