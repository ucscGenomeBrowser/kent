import subprocess, sys
from collections import defaultdict

# based on caddToWig.py in kent/src/hg/makeDb/cadd
# two arguments: input filename and db (one of: hg19, hg38)

def inputLineChunk(fname, db, bedFh):
    " yield all values at consecutive positions as a tuple (chrom, pos, list of (nucl, phred value)) "
    # chr,hg19_pos,grch38_pos,ref,alt,aaref,aaalt,REVEL
    values = []
    firstChrom = None
    firstPos = None
    lastChrom = None
    lastPos = None
    donePos = {}

    assert (db in ["hg19", "hg38"])
    doHg19 = (db=="hg19")

    #fname = "revel_grch38_all_chromosomes.csv.gz"
    if fname.endswith(".gz"):
        ifh = subprocess.Popen(['zcat', fname], stdout=subprocess.PIPE, encoding="ascii").stdout
    else:
        ifh = open(fname)

    for line in ifh:
        if line.startswith("chr"):
            continue
        row = line.rstrip("\n").split(",")

        chrom, hg19Pos, hg38Pos, ref, alt, aaRef, aaAlt, revel, transId = row
        if doHg19:
            pos = hg19Pos
        else:
            pos = hg38Pos

        if not doHg19 and pos==".":
            continue

        pos = int(pos) # wiggle ascii is 1-based AAARRGH!!

        # the file has duplicate values in the hg38 column, but for different hg19 positions!
        if not doHg19:
            if pos in donePos and hg19Pos!=donePos[pos]:
                continue
            hg19Pos = int(hg19Pos)
            donePos[pos] = hg19Pos

        chrom = "chr"+chrom
        revel = float(revel)


        if firstPos is None:
            firstChrom = chrom
            firstPos = pos

        if chrom != lastChrom or pos-lastPos > 1:
            yield firstChrom, firstPos, values
            firstPos, firstChrom = pos, chrom
            values = []
            transIds = {}

            if chrom != lastChrom:
                lastPos = None
                donePos = {}

        if lastPos is None or pos-lastPos > 0:
            values.append([0.0,0.0,0.0,0.0])
            transIds = {}

        nuclIdx = "ACGT".find(alt)

        oldVal = values[-1][nuclIdx]
        if oldVal != 0.0 and oldVal != revel:
            # OK, we know that we have two values at this position for this alt allele now
            # so we write the original transcriptId and the current one to the BED file
            start = int(hg19Pos)-1
            bed = (chrom, start, start+1, alt, "0", ".", start, start+1, transIds[(alt, oldVal)], oldVal)
            bed = [str(x) for x in bed]
            bedFh.write("\t".join(bed))
            bedFh.write("\n")
            bed = (chrom, start, start+1, alt, "0", ".", start, start+1, transId, revel)
            bed = [str(x) for x in bed]
            bedFh.write("\t".join(bed))
            bedFh.write("\n")
            # and only keep the maximum in the bigWig
            revel = max(oldVal, revel)

        values[-1][nuclIdx] = revel
        transIds[(alt, revel)] = transId

        lastPos = pos
        lastChrom = chrom
    # last line of file
    yield firstChrom, firstPos, values

fname = sys.argv[1]
db = sys.argv[2]

outFhs = {
        "A" : open("a.wig", "w"),
        "C" : open("c.wig", "w"),
        "T" : open("t.wig", "w"),
        "G" : open("g.wig", "w")
        }

bedFh = open("overlap.bed", "w")

for chrom, pos, nuclValues in inputLineChunk(fname, db, bedFh):
    if len(nuclValues)==0:
        continue

    for ofh in outFhs.values():
        ofh.write("fixedStep chrom=%s span=1 step=1 start=%d\n" % (chrom, pos))

    for nuclVals in nuclValues:
        for nucIdx in (0,1,2,3):
            alt = "ACGT"[nucIdx]
            ofh = outFhs[alt]
            revel = nuclVals[nucIdx]
            ofh.write(str(revel))
            ofh.write("\n")
