import subprocess, sys
from collections import defaultdict
#from array import array
import numpy as np

# based on revelToWig.py in kent/src/hg/makeDb/scripts/revel/
# three arguments: chrom.sizes, input filename and db (one of: hg19, hg38)

def parseChromSizes(chromSizesFname):
    chromSizes = {}
    for line in open(chromSizesFname):
        chrom, size = line.strip().split()
        size = int(size)
        chromSizes[chrom] = size
    return chromSizes

def initValues(chromSizes, chrom):
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

    if fname.endswith(".gz"):
        ifh = subprocess.Popen(['zcat', fname], stdout=subprocess.PIPE, encoding="ascii").stdout
    else:
        ifh = open(fname)

    chromValues = initValues(chromSizes, "chr1")
    lastChrom = "1"
    for line in ifh:
        if line.startswith("chr"):
            continue
        row = line.rstrip("\n").split(",")[:8]

        chrom, hg19Pos, hg38Pos, ref, alt, aaRef, aaAlt, revel = row
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

        nuclIdx = "ACGT".find(alt)
        chromValues[nuclIdx][pos] = revel

    # last line of file
    yield chrom, chromValues

chromSizesFname = sys.argv[1]
fname = sys.argv[2]

outFhs = {
        0 : open("a.wig", "w"),
        1 : open("c.wig", "w"),
        2 : open("t.wig", "w"),
        3 : open("g.wig", "w")
        }

chromSizes = parseChromSizes(chromSizesFname)

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

