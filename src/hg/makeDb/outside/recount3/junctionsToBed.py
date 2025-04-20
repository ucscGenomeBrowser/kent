#!/usr/bin/env python3
import sys
import csv
import os
import re
import csv
import argparse
import textwrap
from collections import namedtuple
from math import log
sys.path.append('/hive/groups/browser/pycbio/lib')
from pycbio.hgdata.bed import Bed, BedBlock, BedReader, intArraySplit



def main():
	parser = argparse.ArgumentParser(
	formatter_class=argparse.RawDescriptionHelpFormatter,
	description=textwrap.dedent('''\
Converts gff to bed format, adding expression values and CDS from separate files.
Also outputs a decorator bed file, see https://genome.ucsc.edu/goldenPath/help/decorator.html

        '''))
	#group = parser.add_argument_group('required arguments')
	parser.add_argument('--junctions', required=True, type=str, help='annotated gff')
	parser.add_argument('--bed', required=True, type=str, help='output bed')
	parser.add_argument('--decorator', required=True, type=str, help='output decorator bed')
	parser.add_argument('--compilation', required=True, type=str, 
		choices=['ccle', 'gtexv2', 'srav3h', 'tcgav2', 'srav1m'], 
		help='one of ccle, gtexv2, srav3h, tcgav2, srav1m')
	args = parser.parse_args()

	junction_to_bed(args.bed, args.junctions, args.decorator, args.compilation)


def makeSpliceDecorator(bedObj, donor, acc, outFH):
	'''Create decorators for splice consensus'''
	#chr1 1999 2000 green_circle 0 + 1999 2000 0,255,0,255 1 1 0 chr1:1000-2000:feature glyph 0,255,0,255 Circle
	#colorblind color scheme Paul Tol's Muted from 
	# https://www.nceas.ucsb.edu/sites/default/files/2022-06/Colorblind%20Safe%20Color%20Schemes.pdf
	# dark blue = GT donor, AG acceptor (046,037,133), teal= GC donor (093,168,153), faded red = AG/AC (194,106,119) 
	if bedObj.strand == '.':
		# do not decorate
		return
	# assign colors
	splice_map_donor  = {'GT': '046,037,133','GC': '093,168,153','AT': '194,106,119'}
	splice_map_acc = {'AG': '046,037,133', 'AC': '194,106,119'}
	leftColor  = splice_map_donor.get(donor, 'default') 
	rightColor = splice_map_acc.get(acc, 'default') 
	# correct for negative strand
	if bedObj.strand == '-':
		leftColor, rightColor = rightColor, leftColor
	# decorate the first and last two bases of each block
	leftChromStart = bedObj.chromStart
	leftChromEnd = leftChromStart + 2
	leftBlocks = [BedBlock(bedObj.chromStart, bedObj.chromStart+2)]
	rightChromStart = bedObj.chromEnd - 2
	rightChromEnd = bedObj.chromEnd
	rightBlocks = [BedBlock(bedObj.chromEnd-2, bedObj.chromEnd)]
	# format the target location (required for format)
	target='{}:{}-{}:{}'.format(bedObj.chrom, bedObj.chromStart, bedObj.chromEnd, bedObj.name)
	extraCols = [target, 'block', leftColor, 'Circle']
	# create and write bed objects, one for each side
	leftObj = Bed(bedObj.chrom, leftChromStart, leftChromEnd, name='decl'+bedObj.name,
				strand=bedObj.strand, blocks=leftBlocks, numStdCols=12, itemRgb=leftColor,
				extraCols=extraCols)
	leftObj.write(outFH)
	extraCols = [target, 'block', rightColor, 'Circle']
	rightObj = Bed(bedObj.chrom, rightChromStart, rightChromEnd, name='decr'+bedObj.name,
				strand=bedObj.strand, blocks=rightBlocks, numStdCols=12, itemRgb=rightColor,
				extraCols=extraCols)
	rightObj.write(outFH)
     

def junction_to_bed(outputfile, junctions, decoratorfile, compilation):
	'''Parses intron lines, extracts ID info from last field, assigns score'''
	urlbase = '<a href="https://snaptron.cs.jhu.edu/snaptron-studies/jxn2studies?compilation='+compilation
	urlfill = '&jid={jid}&coords={chrom}:{start}-{end}" target="_blank">link to snaptron</a>'
#'https://snaptron.cs.jhu.edu/snaptron-studies/jxn2studies?compilation=<COMPILATION_ID>&jid=<JXN_ID>&coords=<CHROMOSOME:START-END>'
#	url = '<a href="https://snaptron.cs.jhu.edu/gtexv2/snaptron?regions={chrom}:{start}-{end}" target="_blank">{text}</a>'
	decFile = open(decoratorfile, 'wt')
	with open(outputfile, 'wt') as outfile:
		writer = csv.writer(outfile, delimiter='\t', lineterminator=os.linesep)
		# extract all exons from the gff, keep exons grouped by transcript
		maxScore=1
		for line in open(junctions, 'rt'):  
			if line.startswith('#'):
				continue
			[snaptron_id, chrom, start, end, length, strand, annotated, 
				left_motif, right_motif, left_annotated, right_annotated, samples, 
				samples_count, coverage_sum, coverage_avg, coverage_median, 
				source_dataset_id] = line.rstrip().split('\t')
			# skip non-genome chromosomes
			if any(chrom.startswith(prefix) for prefix in ('ERCC', 'SIRV', 'GL', 'JH', 'chrEBV')):
				continue
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
			start = int(start)-1
			end = int(end)
			# score scaling
			score = int(10*log(int(coverage_sum))) # score field only accepts integers
			maxScore = max(score, maxScore)
			extraCols = [coverage_sum, samples_count, donor, acc, link] 
			# create bed object
			bedObj = Bed(chrom, start, end, name=snaptron_id, strand=strand, score=score, 
				thickStart=start, thickEnd=end, numStdCols=9, extraCols=extraCols)
			# create splice site decorator file
			makeSpliceDecorator(bedObj, donor, acc, decFile)
			bedObj.write(outfile)
	decFile.close()
	print('maxScore', maxScore)


if __name__ == "__main__":
	main()
