#!/usr/bin/env python3
import sys
import gzip

def inputLineChunk(infile):
    " yield all values at consecutive positions as a tuple (chrom, pos, list of (nucl, am_pathogenicity)) "
    #CHROM  POS     REF     ALT     genome  uniprot_id      transcript_id   protein_variant am_pathogenicity        am_class
    #chr1    69094   G       T       hg38    Q8NH21  ENST00000335137.4       V2L     0.2937  likely_benign
    values = []  # holds per nucleotide pathogenicity values
    firstChrom = None
    firstPos = None
    lastChrom = None
    lastPos = None

    with gzip.open(infile, 'rt') as f:
    #with open(infile, 'r') as f:
      for line in f:
        if line.startswith("#"):
            continue
        row = line.rstrip("\n").split("\t")
        chrom, pos, ref, alt, genome, uniprot_id, transcript_id, protein_variant, am_pathogenicity, am_class= row
        pos = int(row[1])
        phred = float(am_pathogenicity)

        if firstPos is None:
            firstChrom = chrom
            firstPos = pos

        # if the block position skips one or more bases we must print a wig block and start over
        if chrom!=lastChrom or pos-lastPos > 1 and firstChrom is not None:
            yield firstChrom, firstPos, values
            firstPos, firstChrom = pos, chrom
            #firstPos, firstChrom = None, None # I think this is a bug in the cadd code
            values = []

        # start with an empty array, add numbers if we find values for each allele in subsequent lines
        if pos != lastPos:
            values.append([0.0,0.0,0.0,0.0])
        nuclIdx = "ACGT".find(alt)
        values[-1][nuclIdx] = phred

        lastPos = pos
        lastChrom = chrom
    # last line of file
    yield firstChrom, firstPos, values

# open four wig files, one per nt
outFhs = {
        "A" : open("a.wig", "w"),
        "C" : open("c.wig", "w"),
        "T" : open("t.wig", "w"),
        "G" : open("g.wig", "w")
        }

if len(sys.argv) == 1:
    sys.exit('Please give input file')

for chrom, pos, nuclValues in inputLineChunk(sys.argv[1]):
    if len(nuclValues)==0:
        continue
    # append new entry to all four wig files
    for ofh in outFhs.values():
        ofh.write("fixedStep chrom=%s span=1 step=1 start=%d\n" % (chrom, pos))

    for nuclVals in nuclValues:
        for nucIdx in (0,1,2,3):
            alt = "ACGT"[nucIdx]
            ofh = outFhs[alt]
            phred = nuclVals[nucIdx]
            ofh.write(str(phred))
            ofh.write("\n")
