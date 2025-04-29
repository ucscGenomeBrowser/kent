#!/usr/bin/env python3
import sys
import csv
import os
import re
import argparse
import textwrap
sys.path.append('/hive/groups/browser/pycbio/lib')
from pycbio.hgdata.bed import Bed, BedBlock, intArraySplit

def main():
	parser = argparse.ArgumentParser(
	formatter_class=argparse.RawDescriptionHelpFormatter,
	description=textwrap.dedent('''\
Converts gtf to bed format.

        '''))
	parser.add_argument('--genes', type=str, help='ENST, ENSP, and hugo gene IDs')
	parser.add_argument('--gtf', type=str, required=True, default=None,  help="annotation gtf")
	parser.add_argument('--bed', type=str, required=True, default=None,  help="output bedfile")
	args = parser.parse_args()

	gdict = dict()
	# get HUGO IDs
	with open(args.genes, 'r') as g:
		for line in g:
			ensg, gene, enst, ensp = line.split("\t")
			ensg = enst.split('.')[0]
			gdict[ensg] = gene
			ensp = ensp.strip()
			if ensp != '':
				gdict[ensp.split('.')[0]] = [gene, enst]
                    
	g.close()
	gtf_to_bed(args.bed, args.gtf, gdict)


class Gene(object):
	def __init__(self, itemRgb, tx, chrom, start, end, strand, extrafields):
		self.itemRgb = itemRgb
		self.txID = tx
		self.chrom = chrom
		self.geneStart = start
		self.geneEnd = end
		self.strand = strand
		self.blockList = [BedBlock(start, end)]
		self.extraCols = extrafields
	def add(self, start, end):
		self.geneStart = min(start, self.geneStart)
		self.geneEnd = max(end, self.geneEnd)
		self.blockList.append(BedBlock(start, end))
	def write(self, FH):
		'''Create a Bed object, then write'''
		name = self.txID
		sorted_blocks = sorted(self.blockList, key=lambda x: x.start)
		self.merge(sorted_blocks)
		bedObj = Bed(self.chrom, self.geneStart, self.geneEnd, name=name, strand=self.strand, 
		blocks=self.blocks, itemRgb=self.itemRgb, numStdCols=12, extraCols = self.extraCols)
		bedObj.write(FH)
	def merge(self, sorted_blocks):
		merged_blocks = []
		for block in sorted_blocks:
		        if not merged_blocks:
		            merged_blocks.append(block)
		        else:
		            last_block = merged_blocks[-1]
		            if block.start <= last_block.end:
		                new_end = max(last_block.end, block.end)
		                merged_blocks[-1] = BedBlock(last_block.start, new_end)
		            else:
		                # No overlap, so just add the current block
		                merged_blocks.append(block)
		self.blocks = merged_blocks


def ids_from_gtf(descrField, gdict):
	'''Parse the last field of a gtf to extract transcript and gene IDs'''
	pairs = descrField.split("; ")
	data_dict = {pair.split(" ", 1)[0].strip(): pair.split(" ", 1)[1].strip('"') for pair in pairs}
	# gene_id, transcript_id, gene_type, transcript_type, gene_ens_id, transcript_ens_id, gene_ens_id, gene_parent_id, transcript_parent_id, protein_parent_id
	del data_dict['transcript_type']
	del data_dict['exon']
	# as BED item name we will use the Yale ID (PGOHUMG0000290588) unless there's a HUGO ID
	pgeneName = data_dict['transcript_id']
	# We want the HUGO ID for the pseudogene itself, if present, and for the parent.
	pgeneHugo = 'NA'
	parentHugo = 'NA'
	if data_dict['transcript_ens_id'] in gdict:
		pgeneHugo = gdict[data_dict['transcript_ens_id']]
		pgeneName = pgeneHugo
	elif data_dict['transcript_ens_id'] != 'NA':
		print("WARNING, CANNOT FIND", data_dict['transcript_ens_id']) # doesn't happen
	if data_dict['transcript_parent_id'] in gdict:
		parentHugo = gdict[data_dict['transcript_parent_id']]
	# in some cases the transcript and gene parents are NA
	elif data_dict['protein_parent_id'] in gdict:
		[parentHugo, ensg]  = gdict[data_dict['protein_parent_id']]
		# replace ensg parent with current
		data_dict['transcript_parent_id'] = ensg

	#if hugo == 'NA':
	#	print(data_dict['gene_parent_id'], data_dict['protein_parent_id'])
	# 535 transcripts truly don't have parents so some remain NA
	if data_dict['gene_type'] == 'pseudogene':
		itemRgb = '255,140,0' # dark orange 
		data_dict['gene_type'] = 'unspecified_pseudogene' # clarify
	elif data_dict['gene_type'] == 'unprocessed_pseudogene':
		itemRgb = '0,0,255' # blue
	elif data_dict['gene_type'] == 'processed_pseudogene':
		itemRgb = '85,107,47' # dark olive green 
	extrafields = [pgeneHugo, parentHugo] + list(data_dict.values())
	#extrafields = tuple([hugo, data_dict['gene_type'], data_dict['gene_id'],  
	#	data_dict['gene_ens_id'], data_dict['gene_parent_id'], 
	#	data_dict['protein_parent_id']])
	return itemRgb, pgeneName, extrafields


def gtf_to_bed(outputfile, gtf, gdict):
	geneObj = None
	with open(outputfile, 'wt') as outfile:
		writer = csv.writer(outfile, delimiter='\t', lineterminator=os.linesep)

		prev_transcript, blockstarts, blocksizes, prev_gene, prev_chrom, prev_strand = [None, None, None, None, None, None]
		blockList = []
		# extract all exons from the gtf, keep exons grouped by transcript
		for line in open(gtf):  
			if line.startswith('#'):
				continue
			line = line.rstrip().split('\t')
			chrom, ty, start, end, strand = line[0], line[2], int(line[3]) - 1, int(line[4]), line[6]
			if ty in ['gene', 'transcript', 'UTR', 'start_codon']:
				continue
			# ensembl integrates the chromosome in the ALT and HAP files, we do not
			# so skip these (fortunately they all start with CHR_)
			if chrom.startswith('CHR_'):
				continue

			itemRgb, tx, extrafields = ids_from_gtf(line[8], gdict)
			if geneObj is None:
				geneObj = Gene(itemRgb, tx, chrom, start, end, strand, extrafields)
			elif geneObj.txID == tx:
				if ty == 'exon':
					geneObj.add(start, end)
			# this line is a new gene, so print the previous
			else:
				geneObj.write(outfile)
				geneObj = Gene(itemRgb, tx, chrom, start, end, strand, extrafields)
				cdsObj = None

		# last entry
		geneObj.write(outfile)


if __name__ == "__main__":
    main()
