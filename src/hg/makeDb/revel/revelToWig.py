import subprocess, sys
from collections import defaultdict
#from array import array
import numpy as np
import json

# based on revelToWig.py in kent/src/hg/makeDb/scripts/revel/
# two arguments: chrom.sizes and input filename 

# Yes, the hg19 and hg38 scripts should be merged into one, but I didn't want to mess with the
# hg19 one when it was done, so there are two scripts now. There are hardly ever updates to REVEL
# so this hack may be fine for now.

def parseChromSizes(chromSizesFname):
    chromSizes = {}
    for line in open(chromSizesFname):
        chrom, size = line.strip().split()
        #if chrom!="chr21":
            #continue
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

def outTransIds(chrom, pos, ref, prevTransIds, bedFh):
    " output features of all (alt, score, transId) to bed file. We need to output all we have at this position "
    #print(chrom, pos, ref, alt, revel, prevTransIds)
    start = pos
    chrom = "chr"+chrom
    for alt, scoreTransList in prevTransIds.items():
        tableDict = {}
        tableLines = []
        mouseOvers = []
        allScores = set()
        for revelScore, transIdStr in scoreTransList:
            mouseOvers.append("transcript %s -> score %f" % (transIdStr, revelScore))
            #tableLines.append(transIdStr.replace(",", ", ")+"|"+str(revelScore))
            assert(transIdStr not in tableDict)
            tableDict[transIdStr] = str(revelScore)
            allScores.add(revelScore)

        # ONLY output the line if we have different scores = there is no need to output a feature if all the scores are the same
        if len(allScores)>1:
            #bed = (chrom, start, start+1, ref+">"+alt, "0", ".", start, start+1, ";".join(tableLines), ", ".join(mouseOvers))
            bed = (chrom, start, start+1, ref+">"+alt, "0", ".", start, start+1, "0,0,0", json.dumps(tableDict), ", ".join(mouseOvers))
            bed = [str(x) for x in bed]
            bedFh.write("\t".join(bed))
            bedFh.write("\n")

def inputLineChunk(fname, chromSizes, db):
    " read all values for one chrom and return as four arrays, -1 means 'no value'"
    # chr,hg19_pos,grch38_pos,ref,alt,aaref,aaalt,REVEL
    values = []
    prevTransIds = defaultdict(list)

    if fname.endswith(".gz"):
        ifh = subprocess.Popen(['zcat', fname], stdout=subprocess.PIPE, encoding="ascii").stdout
    else:
        ifh = open(fname)

    #chromValues = initValues(chromSizes, "chr1")
    lastChrom = None
    prevPos = None
    prevRef = None
    doTrans = False
    for line in ifh:
        if line.startswith("chr"):
            continue
        row = line.rstrip("\n").split(",")

        chrom, hg19Pos, hg38Pos, ref, alt, aaRef, aaAlt, revel, transId = row
        transId = transId.replace(";", ",") # hgc outlink code uses , as separator
        if db=="hg38":
            pos = hg38Pos
        else:
            pos = hg19Pos

        if pos==".":
            continue

        pos = int(pos)-1 # wiggle ascii is 1-based AAARRGH!!
        # but I keep everythin 0 based internally
        # the file has duplicate values in the hg38 column, but for different hg19 positions!
        revel = float(revel)

        if pos!=prevPos:

            if doTrans:
                outTransIds(lastChrom, prevPos, prevRef, prevTransIds, bedFh)
            prevTransIds = defaultdict(list)
            doTrans = False

        if chrom != lastChrom:
            if lastChrom!=None:
                yield lastChrom, chromValues
            lastChrom = chrom
            chromValues = initValues(chromSizes, "chr"+chrom)
            prevTransIds = defaultdict(list)
            doTrans = False

        nuclIdx = "ACGT".find(alt)

        oldVal = chromValues[nuclIdx][pos]
        if oldVal != -1 and oldVal != revel:
            # OK, we know that we have at least two different revel values at this position for this alt allele now
            # so we set a flag that for this position, we must output all the alts, their scores, and transIds
            # also, we always use the worst score per position, when there are overlaps
            doTrans = True
            revel = max(oldVal, revel)

        chromValues[nuclIdx][pos] = revel
        prevTransIds[alt].append((revel, transId))
        prevPos = pos
        prevRef = ref
        prevRevel = revel


    # last line of file
    if doTrans:
        outTransIds(lastChrom, prevPos, prevRef, prevTransIds, bedFh)
    yield chrom, chromValues

chromSizesFname = sys.argv[1]
fname = sys.argv[2]
db = sys.argv[3]

outFhs = {
        0 : open("a.wig", "w"),
        1 : open("c.wig", "w"),
        2 : open("g.wig", "w"),
        3 : open("t.wig", "w")
        }

chromSizes = parseChromSizes(chromSizesFname)

bedFh = open("overlap.bed", "w")

for chrom, nuclValues in inputLineChunk(fname, chromSizes, db):
    #print(chrom, nuclValues, len(nuclValues[0]))
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

