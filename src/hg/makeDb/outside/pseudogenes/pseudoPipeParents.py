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

	enst = namedtuple('enst', ['chrom', 'start', 'end', 'strand'])
    # First read the annotation file
    # we will be representing the parent genes as blocks, determining the start and end based on 
    # the gene's transcripts in the annotation file
	# gene ID dict keeping track of starts and ends
	enstDict = dict()
	curGene, start, end, curChr, strand = 'NA', 'NA', 'NA', 'NA', 'NA'
	parentlink = '<a href="hgTracks?db=hg38&position={chrom}:{start}-{end}&parents=pack">{geneID}</a>'
	with open(args.annotbed, 'r') as INF:
		for bedObj in BedReader(INF):
			name = bedObj.name.split('.')[0]
			enstDict[name] = bedObj
    # We will create a bed file with parents and one overlapping block per child
	# based on the pseudogene file: if a transcript has a pseudogene, get its information
    # from enstDict and print a bed line for it, and one per child
    # First we gather the information from the pseudogene file and output a
    # new pseudogene file with a link to the parent transcript (where possible)
	parentDict = dict()
	with open(args.pgenebed, 'r') as INF, open(args.pgeneout, 'w') as OUTF:
		for bedObj in BedReader(INF, numStdCols=12):
			# parent ENST
			parent = bedObj.extraCols[8].split('.')[0]
			if parent == 'NA':
				bedObj.extraCols.append('parent not listed')
				bedObj.write(OUTF)
				continue
			if not parent in enstDict:
				print('ERROR, missing', parent)
				sys.exit()
			# if we find a parent it's time to create a proper object and collect the children
			parentObj = enstDict[parent]
			if parent in parentDict:
				parentDict[parent].add(bedObj)
			else:
				parentDict[parent] = Gene(parent, parentObj, bedObj)
			# add a link to the parent in the child bedObj

			url = parentlink.format(chrom=parentObj.chrom, start=parentObj.start,
				end=parentObj.end, geneID=parent) 
			bedObj.extraCols.append(url)
			bedObj.write(OUTF)
	# now print parent track, adding one block per child for each parent
	with open(args.parentout, 'w') as OUTF2:
		for parentObj in parentDict.values():
			parentObj.write(OUTF2)



class Gene(object):
	'''Parent object'''
	def __init__(self, parent, parentObj, childObj):
		self.name = parent
		self.parentBedObj = parentObj
		self.children = [childObj]
		self.symbol = childObj.extraCols[1]
	def add(self, childObj):
		self.children.append(childObj)
	def write(self, FH):
		'''Create parent and children Bed objects, and print them'''
		parentColor = "128,0,128"   # Dark Purple
		childColor = "192,192,192"  # Gray
		# extrafields are gene(symbol), gene type, and coordinates
		# for the parent, this is a series of coordinates, one per child

		# The children (blocks below the parent) get a mouseover pointing to their location 
		coordURL = '<a href="hgTracks?db=hg38&position={chrom}:{start}-{end}&pseudogenes=pack">'
		coordURL += '{chrom}:{start}-{end}</a>'
		# mouseover in the parent gives the children's IDs with links to their locations
		idURL = '<a href="hgTracks?db=hg38&position={chrom}:{start}-{end}&pseudogenes=pack">'
		idURL += '{childId}</a> '
		# print the children as blocks (we don't know how much they overlap)
		childObjects = []
		coordString = ''
		for childObj in self.children:
			coords_html = coordURL.format(chrom=childObj.chrom, start=childObj.start, end=childObj.end)
			# create bedObject to write
			bedObj = Bed(self.parentBedObj.chrom, self.parentBedObj.chromStart, self.parentBedObj.chromEnd, 
			name=childObj.name, strand=self.parentBedObj.strand, 
			itemRgb=childColor, numStdCols=12, extraCols=[childObj.name, childObj.extraCols[4], coords_html])
			childObjects.append(bedObj)
			# append child coordinate link to string (for adding to the parent)
			coordString += idURL.format(chrom=childObj.chrom, start=childObj.start,
				end=childObj.end, childId=childObj.name)
		# update the parent with children links and print
		self.parentBedObj.extraCols=[self.symbol, 'gene', coordString]
		self.parentBedObj.itemRgb = parentColor
		self.parentBedObj.write(FH)
		# now print children
		[bedObj.write(FH) for bedObj in childObjects]



if __name__ == "__main__":
	main()
