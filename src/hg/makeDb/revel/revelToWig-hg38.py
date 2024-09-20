import subprocess, sys
from collections import defaultdict
#from array import array
import numpy as np

# based on revelToWig.py in kent/src/hg/makeDb/scripts/revel/
# two arguments: chrom.sizes and input filename 

# Yes, the hg19 and hg38 scripts should be merged into one, but I didn't want to mess with the
# hg19 one when it was done, so there are two scripts now. There are hardly ever updates to REVEL
# so this hack may be fine for now.

def parseChromSizes(chromSizesFname):
    chromSizes = {}
    for line in open(chromSizesFname):
        chrom, size = line.strip().split()
        size = int(size)
        chromSizes[chrom] = size
    return chromSizes

def initValues(chromSizes, chrom):
    " return a list of four float arrays with chromSize length and initialized to -1"
    print('init arrays for chrom %s' % chrom)
    arrs = []
    for n in range(0, 4):
        #a = array('f', chromSizes[chrom]*[-1])
        a = np.full(chromSizes[chrom], -1, dtype=float)
        arrs.append(a)
    print('arrays done')
    return arrs

def inputLineChunk(fname, chromSizes):
    " read all values for one chrom and return as four arrays, -1 means 'no value'"
    # chr,hg19_pos,grch38_pos,ref,alt,aaref,aaalt,REVEL
    values = []
    prevTransIds = {}

    if fname.endswith(".gz"):
        ifh = subprocess.Popen(['zcat', fname], stdout=subprocess.PIPE, encoding="ascii").stdout
    else:
        ifh = open(fname)

    chromValues = initValues(chromSizes, "chr1")
    lastChrom = "1"
    for line in ifh:
        if line.startswith("chr"):
            continue
        row = line.rstrip("\n").split(",")

        chrom, hg19Pos, hg38Pos, ref, alt, aaRef, aaAlt, revel, transId = row
        transId = transId.replace(";", ",") # hgc outlink code uses , as separator
        pos = hg38Pos
        if pos==".":
            continue

        pos = int(pos)-1 # wiggle ascii is 1-based AAARRGH!!
        # but I keep everythin 0 based internally
        # the file has duplicate values in the hg38 column, but for different hg19 positions!
        revel = float(revel)

        if chrom != lastChrom and lastChrom!=None:
            yield lastChrom, chromValues
            lastChrom = chrom
            chromValues = initValues(chromSizes, "chr"+chrom)
            prevTransIds = {}

        nuclIdx = "ACGT".find(alt)

        #chromValues[nuclIdx][pos] = revel
        oldVal = chromValues[nuclIdx][pos]
        if oldVal != -1 and oldVal != revel:
            # OK, we know that we have two values at this position for this alt allele now
            # so we write the original transcriptId and the current one to the BED file
            start = pos
            chrom = "chr"+chrom
            bed = (chrom, start, start+1, ref+">"+alt, "0", ".", start, start+1, prevTransIds[(alt, oldVal)], oldVal)
            bed = [str(x) for x in bed]
            bedFh.write("\t".join(bed))
            bedFh.write("\n")
            bed = (chrom, start, start+1, ref+">"+alt, "0", ".", start, start+1, transId, revel)
            bed = [str(x) for x in bed]
            bedFh.write("\t".join(bed))
            bedFh.write("\n")
            # and only keep the maximum in the bigWig
            revel = max(oldVal, revel)

        chromValues[nuclIdx][pos] = revel
        prevTransIds[(alt, revel)] = transId


    # last line of file
    yield chrom, chromValues

chromSizesFname = sys.argv[1]
fname = sys.argv[2]

outFhs = {
        0 : open("a.wig", "w"),
        1 : open("c.wig", "w"),
        2 : open("g.wig", "w"),
        3 : open("t.wig", "w")
        }

chromSizes = parseChromSizes(chromSizesFname)

bedFh = open("overlap.bed", "w")

for chrom, nuclValues in inputLineChunk(fname, chromSizes):
    if len(nuclValues)==0:
        continue

    # need to find positions that have no values at all:
    # so we can distinguish: "has no value" (not covered) with "is reference"
    # (value=0)
    arrSum = np.sum(nuclValues, axis=0)
    # positions without any data have now arrSum[i]==-4

    for nucIdx in (0,1,2,3):
        arr = nuclValues[nucIdx]
        ofh = outFhs[nucIdx]
        lastVal = arr[0]
        stretch = []
        stretchStart = 0
        for i in range(0, arr.size):
            val = arr[i]
            hasData = (arrSum[i]!=-4)

            if val==-1 and hasData:
                if len(stretch)==0:
                    stretchStart = i
                stretch.append(0)
            elif val!=-1:
                if len(stretch)==0:
                    stretchStart = i
                stretch.append(val)
            else:
                # there is no data at all at this position, so flush the block
                if len(stretch)!=0:
                    ofh.write("fixedStep chrom=chr%s span=1 step=1 start=%d\n" % (chrom, stretchStart+1))
                    for revel in stretch:
                        ofh.write(str(revel))
                        ofh.write("\n")
                    ofh.write("\n")
                stretchStart = None
                stretch = []

