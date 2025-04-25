#!/usr/bin/env python3
import sys
import csv
import os
import re
import csv
import argparse
import textwrap
from collections import namedtuple
sys.path.append('/hive/groups/browser/pycbio/lib')
from pycbio.hgdata.bed import Bed, BedBlock, BedReader, intArraySplit

quantInfo = namedtuple('quantInfo', ['topvals', 'quantTable'])


def main():
	parser = argparse.ArgumentParser(
	formatter_class=argparse.RawDescriptionHelpFormatter,
	description=textwrap.dedent('''\
Converts gtf to bed format, adding expression values and CDS from separate files.

        '''))
	required = parser.add_argument_group('required named arguments')
	required.add_argument('gtf', type=str, help='annotated gtf')
	required.add_argument('quant', type=str, help='quantification file')
	required.add_argument('prot', type=str, help='file with ORF info')
	required.add_argument('samples', type=str, help='sample info file')
	required.add_argument('bed', type=str, help='bed output file')
	args = parser.parse_args()

	sampleDict = addSampleInfo(args.samples)
	exprDict = exprValues(args.quant, sampleDict)
	cdsDict = cdsValues(args.prot)
	gtf_to_bed(args.bed, args.gtf, cdsDict, exprDict)


class Gene(object):
	'''Object to gather exons for a transcript; then convert to bed format and write'''
	def __init__(self, data_dict, tx, chrom, start, end, strand, source):
		# these come from the final field of the gtf file
		extrafields = tuple([data_dict['gene_id'], data_dict['gene_name'], data_dict['transcript_id'], 
			data_dict['transcript_name']])
		self.txID = tx
		self.chrom = chrom
		self.geneStart = start
		self.geneEnd = end
		self.strand = strand
		self.blocks = [BedBlock(start, end)]
		self.itemRgb = '0,0,0'
		if source.startswith('TALON'):
			self.itemRgb = '0,0,255'  # blue
		self.extraCols = (source,) + extrafields 
	def add(self, start, end):
		self.geneStart = min(start, self.geneStart)
		self.geneEnd = max(end, self.geneEnd)
		self.blocks.append(BedBlock(start, end))
	def write(self, FH, exprDict, cdsDict):
		'''Create a Bed object, add expression and cdsInfo then write'''
		name = self.txID
		if name in exprDict:
			self.extraCols = self.extraCols + exprDict[name]
		else:
			print('no expression:', name)
			self.extraCols = self.extraCols + tuple(['no expression data', 'no expression data'])
		if name in cdsDict:
			thickStart = cdsDict[name][0]
			thickEnd   = cdsDict[name][1]
		else:
			print('no cds:', name)
			thickStart = thickEnd = self.geneEnd
		self.blocks.sort()
		bedObj = Bed(self.chrom, self.geneStart, self.geneEnd, name=name, strand=self.strand, 
		thickStart=thickStart, thickEnd=thickEnd,
		blocks=self.blocks, itemRgb=self.itemRgb, numStdCols=12, extraCols = self.extraCols)
		bedObj.write(FH)

def cdsValues(cdsfile):
	'''Return dictionary with ORFstart and end tuples'''
	cdsDict = dict()
	f = open(cdsfile, 'r')
	reader = csv.DictReader(f, delimiter='\t')
	for row in reader:
		cdsDict[row['tid']] = [int(row['CDS_Start'])-1, int(row['CDS_Stop'])]
	f.close()
	return cdsDict
	
def addSampleInfo(samplefile):
	'''Clarify sample IDs, adding cell line and alzheimers info'''
	sampleDict = dict()
	f = open(samplefile, 'r')
	reader = csv.DictReader(f, delimiter='\t')
	for row in reader:
		sample = row['sample']
		if row['tissue_or_cell_line'] == "cell_line":
			sampleDict[sample] = f'{sample} (cell line)' 
		elif sample == 'brain_ad':
			sampleDict[sample] = 'brain (Alzheimer\'s)'
		else:
			sampleDict[sample] = sample
	f.close()
	return sampleDict

def exprValues(quantfile, sampleDict):
	'''Create dictionary with transcript ID as key and top values, and all values as namedtuple'''
	exprDict = dict()
	f = open(quantfile, 'r')
	header = f.readline()
	header = header.rstrip().split("\t")[1:]
	# clean up sample IDs
	header = [sampleDict[x] for x in header]
	fieldct = len(header)
	for line in f:
		fields = line.strip().split("\t")
		tx = fields[0]
		values = [round(float(x),2) for x in fields[1:]]
		# create a html table with values for all samples in one field of the bed file
		html_table = '<table>'
		for i in range(fieldct):
			html_table += f'  <tr><td>{header[i]}</td><td>{values[i]}</td></tr>'
		html_table += '</table>'
		# find samples with 10% highest values
		topValString = '0\tNo expression measured'
		named_values = list(zip(header, values))
		maxval = max(values)
		# create two columns: the raw value and the html string
		if maxval > 0:
			threshold = 0.9 * maxval
			max_entries = {column: value for column, value in named_values if value > threshold}
			topValString = f'{maxval}\tMax score(s) in <br>'
			topValString += '<br>'.join([f'{column}: {value}' for column, value in max_entries.items()])
		exprDict[tx] = quantInfo(topvals=topValString, quantTable=html_table)
	f.close()
	return exprDict

def gtf_to_bed(outputfile, gtf, cdsDict, exprDict):
	'''Parses exon lines, extracts ID info from last field, adds CDS and expression info'''
	geneObj = None
	seen = set()
	with open(outputfile, 'wt') as outfile:
		writer = csv.writer(outfile, delimiter='\t', lineterminator=os.linesep)

		prev_transcript, blockstarts, blocksizes, prev_gene, prev_chrom, prev_strand = [None, None, None, None, None, None]
		# extract all exons from the gtf, keep exons grouped by transcript
		for line in open(gtf, 'rt'):  
			if line.startswith('#'):
				continue
			line = line.rstrip().split('\t')
			chrom, source, ty, start, end, strand, descrField = line[0], line[1], line[2], int(line[3]) - 1, int(line[4]), line[6], line[8]
			if chrom == 'chrEBV':  # not in genome
				continue
			if ty in ['gene', 'transcript']:
				continue
			# parse last field for ID
			pairs = descrField.split("; ")
			data_dict = {pair.split(" ", 1)[0].strip(): pair.split(" ", 1)[1].strip('"') for pair in pairs}
			ensg = data_dict['gene_id'].split('.')[0] 
			# transcript_id is unique but can be ugly ("ENST00000459772.5,ENST00000459772.5#2"), 
			# so only use it for the name field
			transcript_id = data_dict['transcript_id']
			if geneObj is None:
				geneObj = Gene(data_dict, transcript_id, chrom, start, end, strand, source)
			elif geneObj.txID == transcript_id:
				if ty == 'exon':
					geneObj.add(start, end)
			# this line is a new gene, so print the previous
			else:
				geneObj.write(outfile, exprDict, cdsDict)
				# start a new gene
				geneObj = Gene(data_dict, transcript_id, chrom, start, end, strand, source)

		# last entry
		geneObj.write(outfile, exprDict, cdsDict)


if __name__ == "__main__":
    main()
