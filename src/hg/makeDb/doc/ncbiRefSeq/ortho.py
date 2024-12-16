#! /cluster/home/jeltje/miniconda3/bin/python3.11

import os 
import csv
import sys
import textwrap
import argparse
from collections import namedtuple
sys.path.append('/hive/groups/browser/pycbio/lib')
from pycbio.hgdata.bed import Bed, BedBlock, BedReader, intArraySplit

parser = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description=textwrap.dedent('''\

Create ortholog tracks from gene_orthologs file, using reformatted gene2accession info
to get UCSC chromosomes. 
The gene_orthologs file starts with human and for any subsequent species it only
lists genes that it didn't already find using human. The tracks refer to each other
so what we're doing here is inferring e.g. dog-mouse orthologs based on the human-dog
and human-mouse orthologs. NCBI does not create gene families, every gene only has one
ortholog per species.
While the ortholog file does list fly, there are no fly-human orthologs listed at time
of writing.

        '''))
group = parser.add_argument_group('required arguments')
group.add_argument('--ortho', type=str, help='gene_ortholog file')
group.add_argument('--species', type=str, help='three column species file (taxID, colloquial, UCSC)o')
group.add_argument('--coords', type=str, help='transcript coordinates and other info')
# optional flag
#parser.add_argument('--myoption', type=str, default=None,  help="DESCRIBE ME")
#parser.add_argument('-d', '--debug', help="Optional debugging output", action='store_true')

class Gene(object):
    def __init__(self, geneID, orthos):
        if orthos == []:
            print('want to create empty list for', geneID)
            sys.exit()
        self.geneID = geneID
        self.orthoList = orthos
    def coordAdd(self, genStart, genStop, representativeRow):
        self.genStart = genStart
        self.genStop = genStop
        self.taxID = representativeRow['#tax_id']
        self.symbol = representativeRow['Symbol']
        self.tx = representativeRow['RNA_nucleotide_accession.version']
        self.chrom = representativeRow['genomic_nucleotide_accession.version']
        self.name = representativeRow['Symbol']
        self.strand = representativeRow['orientation']
    def makeHtml(self, speciesDict, geneDict, blacklist):
        '''Turn coordinate info for ortholog genes into html'''
        # hgTracks?db=hg38&position=chr7:155799980-155812463&ncbiOrtho=pack
        self.orthoList = [gene for gene in self.orthoList if gene not in blacklist]
        self.url = ''
        baselink = '{species}:<a href="hgTracks?db={genome}&position={chrom}:{start}-{end}&ncbiOrtho=pack">{symbol}</a><br>'
        for geneName in self.orthoList:
             if geneName in geneDict:
                 gene = geneDict[geneName]
                 org = speciesDict[gene.taxID]
                 self.url += baselink.format(species=org.colloq, genome=org.ucsc, chrom=gene.chrom, start=gene.genStart, end=gene.genStop, symbol=gene.symbol)
    def write(self, outf):
        '''Create bed object and print'''
        bedObj = Bed(self.chrom, self.genStart, self.genStop, name=self.geneID, strand=self.strand,
                 thickStart=self.genStart, thickEnd=self.genStop,
                 score='0', numStdCols=9, extraCols=[self.name, self.url])
        bedObj.write(outf)


def getOrtholine(infile, speciesDict):
    '''Generator that returns csv lines that match species of interest'''
    with open(infile, 'r') as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            # Only yield rows where the tax IDs are in speciesDict
            if row['#tax_id'] in speciesDict and row['Other_tax_id'] in speciesDict:
                yield row


def makeOrthoSets(infile, speciesDict):
    '''Collect sets of orthologs of interest and make all vs all combinations'''
    orthoRows = []  # This will store rows for the current gene
    prevID = None   # To track the previous GeneID
    geneObjDict = dict()

    # Iterate over rows from the generator
    for row in getOrtholine(infile, speciesDict):
        if prevID is not None and row['GeneID'] != prevID:
            # Process collected rows when a new gene is encountered
            gene_ids = [prevID] + [r['Other_GeneID'] for r in orthoRows]  # Collect all GeneIDs in the current set
            for gene in gene_ids:
                # Create Gene objects with the current set of orthologs
                geneObj = Gene(geneID=gene, orthos=[g for g in gene_ids if g != gene])
                geneObjDict[gene] = geneObj
            # Clear the orthoRows for the next gene
            orthoRows = []
        # Add the current row to orthoRows
        orthoRows.append(row)
        prevID = row['GeneID']  # Update prevID to the current GeneID
    # After the loop, process the last set of rows (if any)
    if orthoRows:
        gene_ids = [r['Other_GeneID'] for r in orthoRows]
        for gene in gene_ids:
            geneObj = Gene(geneID=gene, orthos=[g for g in gene_ids if g != gene])
            geneObjDict[gene] = geneObj
    return geneObjDict

def getGene(infile, geneList):
    '''Generator that returns csv lines that match genes of interest'''
    with open(infile, 'r') as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            if row['GeneID'] in geneList:
                yield row

def addCoords(infile, geneList):
    '''Parse the gene_accession file: from a list of gene rows get the best representatives, 
    genome accession, location, and symbol and add it to the corresponding gene objects'''
    #Extract gene symbols and create gene boundary blocks from multiple lines
    txRows = []  # This will store rows for the current gene
    prevID = None   # To track the previous GeneID
    # Iterate over rows from the generator
    for row in getGene(infile, geneList):
        if prevID is not None and row['GeneID'] != prevID:
            # Process collected rows when a new gene is encountered
            addGeneInfo(geneList[prevID], txRows)
            # Clear the txRows for the next gene
            txRows = []
        # Add the current row to orthoRows
        txRows.append(row)
        prevID = row['GeneID']  # Update prevID to the current GeneID
    # process final set
    if txRows:
        addGeneInfo(geneList[prevID], txRows)


def addGeneInfo(geneObj, rows):
    '''From a set of rows for one gene, get the best representatives, genome accession, location, 
     and symbol; add this to the gene object'''
    #returnObjects = dict()
    best = ['REVIEWED', 'VALIDATED', 'PROVISIONAL', 'INFERRED']
    ok = ['PREDICTED', 'MODEL']
    selected = []
    for status in best:
        selected = [row for row in rows if row['status'] in [best]]
    if len(selected) == 0:
        # if we don't have a set of genes now, see if we have ok genes
        for status in ok:
            selected = [row for row in rows if row['status'] in [ok]]
    if len(selected) == 0:
        # all tx for this gene have status '-'
        selected = rows
    
    chroms = set(row['genomic_nucleotide_accession.version'] for row in selected)
    # there are duplicate IDs on chrX and Y TODO: make unique IDs
    for ch in chroms:
        subselect = [row for row in selected if row['genomic_nucleotide_accession.version'] == ch]
        # be aware we're only taking starts and stops of the best set, not of all tx
        genStart = min([int(row['start_position_on_the_genomic_accession']) for row in subselect])
        genStop = max([int(row['end_position_on_the_genomic_accession']) for row in subselect])
        geneObj.coordAdd(genStart, genStop, subselect[0])

if len(sys.argv)==1:
    parser.print_help()
    sys.exit(1)
args = parser.parse_args()

# Main
# get tax_id, ucsc name and common name for each species we want tracks for
# we're making all vs all tracks
speciesDict = dict()
species = namedtuple('species', ['colloq', 'ucsc']) 
with open(args.species, 'r') as f:
    for line in f:
        ncbi, colloq, ucsc = line.strip().split('\t')
        speciesDict[ncbi] = species(colloq, ucsc)

# parse the ortholog file and create gene objects that have a gene ID and a set of orthologs
geneDict = makeOrthoSets(args.ortho, speciesDict)

# geneDict has a gene ID as key and a gene Object as value
# now we know which genes we want, extract their coordinate info and add
addCoords(args.coords, geneDict)

#import inspect
#for gene in geneDict:
#  for name, value in inspect.getmembers(geneDict[gene]):
#    if not name.startswith('__') and not callable(value):
#        print(f"{name}: {value}") 
#  sys.exit()

# sanity check
# blacklist the genes for which we didn't find info
blacklist = [k for k, v in geneDict.items() if getattr(v, 'taxID', None) is None]
print('There are', len(blacklist), 'genes without genome info', file=sys.stderr)
# remove these from the set (the blacklist is needed for orthologs)
geneDict = {k: v for k, v in geneDict.items() if getattr(v, 'taxID', None) is not None}

# for each organism, print a bed file
for tax_id in speciesDict:
    organism = speciesDict[tax_id]
    organismGenes = {k:v for k, v in geneDict.items() if v.taxID == tax_id}
    outf = open(f'{organism.ucsc}.bed', 'w')
    for gene in organismGenes.values():
        gene.makeHtml(speciesDict, geneDict, blacklist)
        gene.write(outf)
    outf.close()
