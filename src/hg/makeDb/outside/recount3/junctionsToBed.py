#!/usr/bin/env python3
######################################################################
# DO NOT CODE REVIEW, SINGLE USE ONLY, CHECK recount3 TRACKS INSTEAD #
######################################################################

import sys
import argparse
import textwrap
from math import log
import pipettor
sys.path.append('/hive/groups/browser/pycbio/lib')
from pycbio.hgdata.bed import Bed
from pycbio.sys import fileOps
from pycbio.sys.svgcolors import SvgColors

def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent('''
        Converts gff to bed format, adding expression values and CDS from separate files.
        Also outputs a decorator bed file, see https://genome.ucsc.edu/goldenPath/help/decorator.html
        '''))
    parser.add_argument('--junctions', required=True, type=str, help='annotated gff')
    parser.add_argument('--bed', required=True, type=str, help='output bed')
    parser.add_argument('--compilation', required=True, type=str,
                        choices=['ccle', 'gtexv2', 'srav3h', 'tcgav2', 'srav1m'],
                        help='one of ccle, gtexv2, srav3h, tcgav2, srav1m')
    args = parser.parse_args()

    junction_to_bed(args.bed, args.junctions, args.compilation)

# base color with min/max HSV value
BASE_COLORS = {
    ('GT', 'AG'): (SvgColors.deepskyblue, 0.70, 1.00),  # U2 common
    ('GC', 'AG'): (SvgColors.turquoise, 0.70, 1.00),  # U2 rare
    ('AT', 'AC'): (SvgColors.orange, 0.70, 1.00),  # U12
}
UNKNOWN_BASE_COLOR = (SvgColors.darkgrey, 0.25, 0.85)

# estimated from look at output from running this program
MAX_SCORE = 200

def item_color(donor, acc, score):
    base_color, value_min, value_max = BASE_COLORS.get((donor, acc), UNKNOWN_BASE_COLOR)
    norm_score = score / MAX_SCORE
    value = value_max - (norm_score * (value_max - value_min))
    return base_color.setValue(value)

def process_rec(compilation, line, outfile):
    urlbase = '<a href="https://snaptron.cs.jhu.edu/snaptron-studies/jxn2studies?compilation=' + compilation
    urlfill = '&jid={jid}&coords={chrom}:{start}-{end}" target="_blank">link to snaptron</a>'
    [snaptron_id, chrom, start, end, length, strand, annotated,
        left_motif, right_motif, left_annotated, right_annotated, samples,
        samples_count, coverage_sum, coverage_avg, coverage_median,
        source_dataset_id] = line.rstrip().split('\t')
    # skip non-genome chromosomes
    if any(chrom.startswith(prefix) for prefix in ('ERCC', 'SIRV', 'GL', 'JH', 'chrEBV')):
        return 0
    # turn left and right motifs into donor and acceptor
    donor = left_motif.upper()
    acc = right_motif.upper()
    if strand == '?':
        strand = '.'
    elif strand == '-':
        complement = {'A': 'T', 'T': 'A', 'C': 'G', 'G': 'C'}
        donor = ''.join(complement[base] for base in reversed(acc))
        acc = ''.join(complement[base] for base in reversed(left_motif.upper()))
    # link out to sample information for each junction
    link = urlbase + urlfill.format(jid=snaptron_id, chrom=chrom, start=start, end=end)
    # correct coordinates
    start = int(start) - 1
    end = int(end)
    # score scaling
    score = int(10 * log(int(coverage_sum)))  # score field only accepts integers
    size = end - start
    color = item_color(donor, acc, score).toRgb8Str()
    extraCols = [size, coverage_sum, samples_count, donor, acc, link, donor + '/' + acc]
    # create bed object
    bedObj = Bed(chrom, start, end, name=snaptron_id, strand=strand, score=score,
                 thickStart=start, thickEnd=end, numStdCols=9, itemRgb=color,
                 extraCols=extraCols)
    bedObj.write(outfile)
    return score

def junction_to_bed(outputfile, junctions, compilation):
    '''Parses intron lines, extracts ID info from last field, assigns score'''

    # 'https://snaptron.cs.jhu.edu/snaptron-studies/jxn2studies?compilation=<COMPILATION_ID>&jid=<JXN_ID>&coords=<CHROMOSOME:START-END>'
    #   url = '<a href="https://snaptron.cs.jhu.edu/gtexv2/snaptron?regions={chrom}:{start}-{end}" target="_blank">{text}</a>'

    bed_sort_cmd = ('sort', '-k1,1', '-k2,2n')
    with pipettor.Popen(bed_sort_cmd, 'wt', stdout=outputfile) as outfile:
        # extract all exons from the gff, keep exons grouped by transcript
        maxScore = 0
        infh = fileOps.opengz(junctions, 'rt')
        for line in infh:
            if line.startswith('#'):
                continue
            score = process_rec(compilation, line, outfile)
            maxScore = max(maxScore, score)
    print('maxScore', maxScore)


if __name__ == "__main__":
    main()
