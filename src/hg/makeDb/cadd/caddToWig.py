import subprocess

def inputLineChunk():
    " yield all values at consecutive positions as a tuple (chrom, pos, list of (nucl, phred value)) "
    ## CADD GRCh37-v1.6 (c) University of Washington, Hudson-Alpha Institute for Biotechnology and Berlin Institute of Health 2013-2019. All rights reserved.
    #Chrom  Pos     Ref     Alt     RawScore        PHRED
    #1       10001   T       A       0.088260        4.066
    values = []
    firstChrom = None
    firstPos = None
    lastChrom = None
    lastPos = None

    #for line in gzip.open("whole_genome_SNVs.tsv.gz"):
    for line in subprocess.Popen(['zcat', "whole_genome_SNVs.tsv.gz"], stdout=subprocess.PIPE, encoding="ascii").stdout:
        if line.startswith("#"):
            continue
        row = line.rstrip("\n").split("\t")
        chrom, pos, ref, alt, raw, phred = row
        chrom = "chr"+chrom
        pos = int(row[1])
        phred = float(phred)

        if firstPos is None:
            firstChrom = chrom
            firstPos = pos

        if chrom!=lastChrom or pos-lastPos > 1 and firstChrom is not None:
            yield firstChrom, firstPos, values
            firstPos, firstChrom = None, None
            values = []

        if pos != lastPos:
            values.append([0.0,0.0,0.0,0.0])
        nuclIdx = "ACGT".find(alt)
        values[-1][nuclIdx] = phred

        lastPos = pos
        lastChrom = chrom
    # last line of file
    yield firstChrom, firstPos, values

outFhs = {
        "A" : open("a.wig", "w"),
        "C" : open("c.wig", "w"),
        "T" : open("t.wig", "w"),
        "G" : open("g.wig", "w")
        }

for chrom, pos, nuclValues in inputLineChunk():
    if len(nuclValues)==0:
        continue

    for ofh in outFhs.values():
        ofh.write("fixedStep chrom=%s span=1 step=1 start=%d\n" % (chrom, pos))

    for nuclVals in nuclValues:
        for nucIdx in (0,1,2,3):
            alt = "ACGT"[nucIdx]
            ofh = outFhs[alt]
            phred = nuclVals[nucIdx]
            ofh.write(str(phred))
            ofh.write("\n")
