#!/usr/bin/env python3
import sys
import csv
import os
import re
import argparse
import textwrap
from collections import namedtuple
sys.path.append('/hive/groups/browser/pycbio/lib')
from pycbio.hgdata.bed import Bed, BedBlock, intArraySplit, BedReader

def main():
	parser = argparse.ArgumentParser(
	formatter_class=argparse.RawDescriptionHelpFormatter,
	description=textwrap.dedent('''\
Read pgenes bed file and annotation files, construct parent track with links to child tracks.
Maybe update pgenes bed with parent links.

        '''))
	parser.add_argument('--pgenebed', type=str, help='ENST, ENSP, and hugo gene IDs')
	parser.add_argument('--annotbed', type=str, required=True, default=None,  help="annotation gtf, sorted by name then chrom")
	parser.add_argument('--pgeneout', type=str, required=True, default=None,  help="output pgene bedfile")
	parser.add_argument('--parentout', type=str, required=True, default=None,  help="output parent bedfile")
	args = parser.parse_args()

	ensg = namedtuple('ensg', ['chrom', 'start', 'end', 'strand'])
	# gene ID dict keeping track of starts and ends
	ensgDict = dict()
	curGene, start, end, curChr, strand = 'NA', 'NA', 'NA', 'NA', 'NA'
	parentlink = '<a href="hgTracks?db=hg38&position={chrom}:{start}-{end}&parents=pack">{geneID}</a>'
	with open(args.annotbed, 'r') as INF:
		for bedObj in BedReader(INF, numStdCols=6):
			if bedObj.name == curGene:
				if(bedObj.chrom) == curChr:  # skip chrY pseudoautosomal (for now?)
					start = min(bedObj.start, int(start))
					end = max(bedObj.end, int(end))
				#else:
				#	print('skipping', bedObj, file=sys.stderr)
			else:
				ensgDict[curGene] = ensg(curChr, start, end, strand)
				start = int(bedObj.start)
				end = int(bedObj.end)
				strand = bedObj.strand
				curChr = bedObj.chrom
				curGene = bedObj.name
		# final one
		ensgDict[curGene] = ensg(curChr, start, end, strand)

	# if a gene has a pseudogene, put it in the parent set
	parentDict = dict()
	with open(args.pgenebed, 'r') as INF, open(args.pgeneout, 'w') as OUTF:
		for bedObj in BedReader(INF, numStdCols=12):
			# parent ENSG
			parent = bedObj.extraCols[4]
			if parent == 'NA':
				bedObj.extraCols[4] = 'parent not listed'
				bedObj.write(OUTF)
				continue
			if not parent in ensgDict:
				print('ERROR, missing', parent)
				sys.exit()
			# if we find a parent it's time to create a proper object and collect the children
			parentTuple = ensgDict[parent]
			if parent in parentDict:
				parentDict[parent].add(bedObj)
			else:
				parentDict[parent] = Gene(parent, parentTuple, bedObj)
			# add a link to the parent in the child bedObj

			url = parentlink.format(chrom=parentTuple.chrom, start=parentTuple.start,
				end=parentTuple.end, geneID=parent) 
			# replace the parent ID with the URL to it
			bedObj.extraCols[4] = url
			bedObj.write(OUTF)
	# now print parent track, adding one block per child for each parent
	with open(args.parentout, 'w') as OUTF2:
		for parentObj in parentDict.values():
			parentObj.write(OUTF2)



class Gene(object):
	def __init__(self, parent, parentTuple, childObj):
		self.name = parent
		self.chrom = parentTuple.chrom
		self.geneStart = parentTuple.start
		self.geneEnd = parentTuple.end
		self.strand = parentTuple.strand
		self.children = [childObj]
		self.symbol = childObj.extraCols[0]
	def add(self, childObj):
		self.children.append(childObj)
	def write(self, FH):
		'''Create parent and children Bed objects, and print them'''
		parentColor = "128,0,128"   # Dark Purple
		childColor = "192,192,192"  # Gray
		# mouseover link for children
		baselink = '{pgenetype} <a href="hgTracks?db=hg38&position={chrom}:{start}-{end}&pseudogenes=pack">'
		baselink += '{geneID}</a> ({chrom}:{start}-{end})'
		# let's make the parent wide and the children narrow
		bedObj = Bed(self.chrom, self.geneStart, self.geneEnd, name=self.name, strand=self.strand, 
		thickStart=self.geneStart, thickEnd=self.geneEnd, itemRgb=parentColor, numStdCols=9, 
		extraCols=[self.symbol, 'N/A'])
		bedObj.write(FH)
		for childObj in self.children:
			url = baselink.format(pgenetype=childObj.extraCols[1], chrom=childObj.chrom, start=childObj.start, 
				end=childObj.end, geneID=childObj.name)
			bedObj = Bed(self.chrom, self.geneStart, self.geneEnd, name=childObj.name, strand=self.strand, 
			itemRgb=childColor, numStdCols=9, extraCols=['', url])
			bedObj.write(FH)



if __name__ == "__main__":
    main()
