#!/usr/bin/env python3

"""Read genoflu-multi.py's output results.tsv and a TSV mapping of sample names and segments to
full sequence names.  Output TSV mapping full sequence name to the genoflu-assigned genotype
and segment type (if any)."""

import argparse, sys, re, csv

seg_order = ['PB2', 'PB1', 'PA', 'HA', 'NP', 'NA', 'MP', 'NS']

def parseCommandLine():
    parser = argparse.ArgumentParser()
    parser.add_argument('genofluTsv', help="results.tsv file from genoflu-multi")
    parser.add_argument('sampleSegToFullTsv', help="TSV file mapping sample name and segment number to full seq name")
    parser.add_argument('outTsv', help="Output TSV file mapping full seq name to genotype and segment type")
    args = parser.parse_args()
    return args

def parseGenotype(gt):
    """If it's a verbose "Not assigned: ..." then just return the empty string."""
    if gt.startswith('Not assigned:'):
        return ''
    else:
        return gt


def genoFluToSegmentMetadata(genofluTsv, sampleSegToFullTsv, outTsv):
    sample_seg_to_full = dict()
    for line in sampleSegToFullTsv:
        sample, seg_num, full_name = line.split('\t')
        if seg_num in seg_order:
            seg_name = seg_num
        else:
            seg_name = seg_order[int(seg_num) - 1]
        sample_seg_to_full[sample + ':' + seg_name] = full_name.strip()
    gf_reader = csv.DictReader(genofluTsv, delimiter='\t')
    print('\t'.join(['name', 'genoflu_genotype', 'genoflu_segtype']), file=outTsv)
    for row in gf_reader:
        sample = row['Strain']
        gt = parseGenotype(row['Genotype'])
        seg_types = row['Genotype List Used, >=98%'].split(', ')
        for seg_name_type in seg_types:
            if seg_name_type:
                seg_name, seg_type = seg_name_type.split(':')
                full_name = sample_seg_to_full.get(sample + ':' + seg_name)
                if full_name:
                    print('\t'.join([ full_name, gt, seg_type]), file=outTsv)


def main():
    args = parseCommandLine()
    with open(args.genofluTsv, 'r') as genofluTsv:
        with open(args.sampleSegToFullTsv, 'r') as sampleSegToFullTsv:
            with open(args.outTsv, 'w') as outTsv:
                genoFluToSegmentMetadata(genofluTsv, sampleSegToFullTsv, outTsv)

if __name__ == "__main__":
    main()
