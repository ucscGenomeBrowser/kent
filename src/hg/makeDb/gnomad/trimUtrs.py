#!/usr/bin/env python3

"""
Given a 1-based amino acid range, and a list of genomic exon positions (from a genePred
or bed12), convert the amino acid range to the cds positions.

Example:
#geneName transcriptName aa range (200 amino acid length protein)
GENE1 Transcript1 1-200 extra1 extra2

#chrom chromStart chromEnd exonSizes exonStarts (a gene with 4 exons, 1002bp total coding exons)
chr1 1 1003 12,350,238,402 0,30,40,400,600

output (the aa string covers the first 3 coding exons):
chr1 1 639 12,350,238 0,30,400
"""

import sys
import argparse
from collections import defaultdict,namedtuple

def commandLine():
    parser = argparse.ArgumentParser(
        description="Convert amino acids ranges to genomic coordinates, writes to stdout. One of transcripts or aaRanges can be stdin",
        prefix_chars="-", usage="%(prog)s [options]", add_help=True)
    parser.add_argument("transcripts", action="store", help="bed12 of transcripts, use '-' for stdin")
    parser.add_argument("-v", "--verbose", action="store_true", default=False, help="Turn verbose logging on, writes to stderr")

    args = parser.parse_args()
    return args

# struct for keeping track of transcripts and their exon coordinates and sizes
txDict = defaultdict(list)

bedFields = ["chrom", "chromStart", "chromEnd", "name", "score", "strand",
"thickStart", "thickEnd", "itemRgb", "exonCount", "exonSizes", "exonStarts"]
# autoSql map of the output bed for a conversion to bigBed later:
bedInfo = namedtuple('bedFields',  bedFields)

def log(msg):
    """Write message to stderr instead of stdout."""
    sys.stderr.write(msg + "\n")

def logBedRecord(ret):
    sys.stderr.write("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t0,0,0\t%s\t%s\t%s\n" % (ret.chrom, ret.chromStart,
        ret.chromEnd, ret.name, ret.score, ret.strand, ret.thickStart, ret.thickEnd,
        ret.exonCount, ",".join([str(x) for x in ret.exonSizes]),
        ",".join([str(x) for x in ret.exonStarts])))

def printBedRecord(ret):
    print("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t0,0,0\t%s\t%s\t%s" % (ret.chrom, ret.thickStart,
        ret.thickEnd, ret.name, ret.score, ret.strand, ret.thickStart, ret.thickEnd,
        ret.exonCount, ",".join([str(x) for x in ret.exonSizes]),
        ",".join([str(x) for x in ret.exonStarts])))

def sanityCheckBed12(ret, original=None, starts=False):
    """Check that a bed12 follows the usual bed rules. Exit if a bad record"""
    def logOriginal():
        if original:
            log("original bed")
            logBedRecord(original)

    if len(ret.exonSizes) != int(ret.exonCount):
        logOriginal()
        log("ERROR: len(exonSizes) != exonCount")
        logBedRecord(ret)
        sys.exit(1)
    if len(ret.exonStarts) != int(ret.exonCount):
        logOriginal()
        log("ERROR: len(exonStarts) != exonCount")
        logBedRecord(ret)
        sys.exit(1)
    for i in range(ret.exonCount):
        if i > 0:
            if int(ret.exonStarts[i]) <= (int(ret.exonStarts[i-1]) + int(ret.exonSizes[i-1])):
                logOriginal()
                log("ERROR: exon blocks must be in ascending order. block %d and %d overlap." % (i-1, i))
                logBedRecord(ret)
                sys.exit(1)
        if (ret.exonStarts[i] < 0 or ret.exonSizes[i] < 0 or
                int(ret.chromStart) + int(ret.exonStarts[i]) >= int(ret.chromEnd)):
            logOriginal()
            log("ERROR trimming %s: %d + %d (%d) >= %d for '%s'" %
                ("starts" if starts else  "ends", int(ret.chromStart), int(ret.exonStarts[i]),
                int(ret.chromStart) + int(ret.exonStarts[i]), int(ret.chromEnd), ret.name))
            logBedRecord(ret)
            sys.exit(1)
    try:
        if (ret.chromStart + ret.exonSizes[-1] + ret.exonStarts[-1] != ret.chromEnd):
            logOriginal()
            log("ERROR: chromStart + exonSizes[-1] + exonStarts[-1] != chromEnd")
            log("%d + %d + %d (%d) != %d" % (ret.chromStart, ret.exonSizes[-1], ret.exonStarts[-1], ret.chromStart + ret.exonStarts[-1] + ret.exonSizes[-1], ret.chromEnd))
            logBedRecord(ret)
            sys.exit(1)
    except IndexError:
        logOriginal()
        log("ERROR: no exonSizes or exonStarts")
        logBedRecord(ret)
        sys.exit(1)

def lineToBed(fields, lineNum=None):
    """Transform bed12 line to namedtuple bedFields."""
    assert(len(fields) == 12)
    chrom = fields[0]
    chromStart = int(fields[1])
    chromEnd = int(fields[2])
    name = fields[3]
    score = fields[4]
    strand = fields[5]
    thickStart = int(fields[6])
    thickEnd = int(fields[7])
    itemRgb = fields[8]
    exonCount = int(fields[9])
    exonSizes = [int(x) for x in fields[10].strip(',').split(",")]
    exonStarts = [int(x) for x in fields[11].strip(',').split(",")]
    ret = bedInfo(chrom, chromStart, chromEnd, name, score, strand, thickStart, thickEnd,
        itemRgb, exonCount, exonSizes, exonStarts)
    sanityCheckBed12(ret)
    return ret

def trimUtrs(fields, verbose=False, lineNum=None):
    """Fix up the chromStart, chromEnd, thickStart, thickEnd and blocks of a bed12
        to only include the coding regions. Return non-transformed for non-coding transcripts."""
    if fields.thickStart == fields.thickEnd:
        return fields
    if fields.chromStart == fields.thickStart and fields.chromEnd == fields.thickEnd:
        return fields
    
    retSizes = [x for x in fields.exonSizes]
    retStarts = [x for x in fields.exonStarts]
    retExonCount = 0
    exonStartPos = 0
    exonEndPos = 0
    chromStart = fields.chromStart
    chromEnd = fields.chromEnd

    if fields.strand != "+" and fields.strand != "-":
        # in general not a big deal to be missing strand, but later for mapping amino
        # acids we need a strand to figure out which side to start from
        if verbose:
            log("WARNING: no strand information for '%s'" % fields.name)
        return

    if fields.chromStart < fields.thickStart:
        diff = fields.thickStart - fields.chromStart
        for i in range(fields.exonCount):   
            exonStartPos = fields.chromStart + retStarts[i]
            exonEndPos = exonStartPos + retSizes[i]
            # this exon contains the start of the coding region (or end if negative stranded)
            # and so will become the first block
            if exonStartPos <= fields.thickStart and exonEndPos > fields.thickStart:
                chromStart = fields.thickStart
                retSizes = retSizes[i:]
                retSizes[0] -= (fields.thickStart - exonStartPos)
                retStarts = [0] + retStarts[i+1:]
                for ix in range(1, len(retStarts)):
                    # subtract the old chromStarts by how far we moved up
                    retStarts[ix] -= diff
                break

    if fields.chromEnd > fields.thickEnd:
        diff = fields.chromEnd - fields.thickEnd
        # walk backwards over exon block indices
        for i in range(len(retStarts)-1,-1,-1):   
            exonStartPos = chromStart + retStarts[i]
            exonEndPos = exonStartPos + retSizes[i]
            # this exon is where coding region ends, and will become the last block
            if exonStartPos < fields.thickEnd and exonEndPos >= fields.thickEnd:
                chromEnd = fields.thickEnd
                retSizes = retSizes[:i+1]
                retSizes[i] -= exonEndPos - fields.thickEnd
                retStarts = retStarts[:i+1]
                break

    ret = bedInfo(fields.chrom, chromStart, chromEnd, fields.name, 0, fields.strand,
        chromStart, chromEnd, "0,0,0", len(retSizes), retSizes, retStarts)
    sanityCheckBed12(ret, fields, False)
    return ret

def txToDict(txFh, verbose=False):
    """Make a dict of all the transcripts for lookup later."""
    lineCount = 1
    for line in txFh:
        splitLine = line.strip().split('\t')
        bed = lineToBed(splitLine, lineCount)
        if bed.thickStart == bed.thickEnd:
            continue
        fixed = trimUtrs(bed, verbose, lineCount)
        txDict[splitLine[3]].append(fixed)
        lineCount += 1

def main():
    """open up the input genePred or bed12 and the input aa strings and call the
       converter."""
    args = commandLine()
    verbose = args.verbose
    txFname = args.transcripts

    if txFname is not "-":
        with open(txFname) as f:
            txToDict(f)
    else:
        txToDict(sys.stdin)
    if verbose:
        log("%d transcript added to transcript dict" % len(txDict))
    for tx in txDict:
        for item in txDict[tx]:
            printBedRecord(item)
    sys.exit(0)

if __name__ == "__main__":
    main()
