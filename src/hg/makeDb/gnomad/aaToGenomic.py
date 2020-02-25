#!/usr/bin/env python3

"""
Given a 1-based amino acid range, and a bed12 of transcripts,
convert the amino acid range to a genomic range with exon blocks.

This program is currently specialized to the gnomad missense constraint
track, but can be more generalized if the score conversion part is
commented out.

Example:
#transcript gene chrom range genomic_start genomic_end obs_mis exp_mis obs_exp chiseq_null_diff region
ENST00000337907.3   RERE    1   1-507   8716356 8424825 97  197.9807    0.489947    51.505535   RERE_1

chr1    8424824 8716356 ENST00000337907.3   0.49    -   8424824 8716356 224,165,8   13  74,163,81,99,100,125,49,105,97,106,126,71,325   0,1047,57962,101160,130298,132640,143861,176448,191709,192652,249795,259544,291207

Here the itemRgb column corresponds to the color scheme used at the decipher browser.
"""

import sys
import argparse
from collections import defaultdict,namedtuple
from scipy.stats import chi2

def commandLine():
    parser = argparse.ArgumentParser(
        description="Converts amino acids ranges to genomic coordinates, writes to stdout. One of transcripts or aaRanges can be stdin",
        prefix_chars="-", usage="%(prog)s [options]", add_help=True)
    parser.add_argument("transcripts", action="store", help="bed12 of transcripts, use '-' for stdin")
    parser.add_argument("aaRanges", action="store", help="aa range file, use '-' for stdin")
    parser.add_argument("-v", "--verbose", action="store_true", default=False, help="Turn verbose logging on, writes to stderr")

    args = parser.parse_args()
    if args.transcripts == "-" and args.aaRanges == "-":
        sys.stderr.write("ERROR: Only one of transcripts and aaRanges can be stdin")
        sys.exit(1)

    return args

# struct for keeping track of transcripts and their exon coordinates and sizes
# use a defaultdict of list so PAR regions can have the different transcripts
txDict = defaultdict(list)

bedFields = ["chrom", "chromStart", "chromEnd", "name", "score", "strand",
"thickStart", "thickEnd", "itemRgb", "exonCount", "exonSizes", "exonStarts"]
# autoSql map of the output bed for a conversion to bigBed later:
bedInfo = namedtuple('bedFields',  bedFields)

def log(msg):
    """Write message to stderr instead of stdout."""
    sys.stderr.write(msg + "\n")

def logBedRecord(ret):
    sys.stderr.write("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (ret.chrom,
        ret.chromStart, ret.chromEnd, ret.name, ret.score, ret.strand, ret.thickStart,
        ret.thickEnd, ret.itemRgb, ret.exonCount, ",".join([str(x) for x in ret.exonSizes]),
        ",".join([str(x) for x in ret.exonStarts])))

def printBedRecord(ret):
    print("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" % (ret.chrom, ret.thickStart,
        ret.thickEnd, ret.name, ret.score, ret.strand, ret.thickStart, ret.thickEnd, ret.itemRgb,
        ret.exonCount, ",".join([str(x) for x in ret.exonSizes]),
        ",".join([str(x) for x in ret.exonStarts])))

def bedToStr(ret):
    """Return the bedFields object as a string"""
    return "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" % (ret.chrom,
        ret.chromStart, ret.chromEnd, ret.name, ret.score, ret.strand, ret.thickStart,
        ret.thickEnd, ret.itemRgb, ret.exonCount, ",".join([str(x) for x in ret.exonSizes]),
        ",".join([str(x) for x in ret.exonStarts]))

def sanityCheckBed12(ret, original=None):
    """Check that a bed12 follows the usual bed rules. Exit if a bad record"""
    def logOriginal():
        # helper function for logging the input bed line
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

    try:
        for i in range(ret.exonCount):
            # check ascending order of blocks
            if i > 0:
                if int(ret.exonStarts[i]) <= (int(ret.exonStarts[i-1]) + int(ret.exonSizes[i-1])):
                    logOriginal()
                    log("ERROR: exon blocks must be in ascending order. block %d and %d overlap." %
                        (i-1, i))
                    logBedRecord(ret)
                    sys.exit(1)
            # check for no zero-length blocks
            # zero length blocks are ok in general but not for this bed
            if (ret.exonStarts[i] < 0 or ret.exonSizes[i] <= 0 or
                    int(ret.chromStart) + int(ret.exonStarts[i]) >= int(ret.chromEnd)):
                logOriginal()
                log("ERROR: invalid exonStart or exonSize")
                logBedRecord(ret)
                sys.exit(1)
        # check that blocks span chromStart to chromEnd
        if (ret.chromStart + ret.exonSizes[-1] + ret.exonStarts[-1] != ret.chromEnd):
            logOriginal()
            log("ERROR: chromStart + exonSizes[-1] + exonStarts[-1] != chromEnd")
            log("%d + %d + %d (%d) != %d" % (ret.chromStart, ret.exonSizes[-1], ret.exonStarts[-1],
                ret.chromStart + ret.exonStarts[-1] + ret.exonSizes[-1], ret.chromEnd))
            logBedRecord(ret)
            sys.exit(1)
    except IndexError:
        logOriginal()
        log("ERROR: no exonSizes or exonStarts")
        logBedRecord(ret)
        sys.exit(1)

def checkAaLenToBlockLen(ret, original, cdnaEndPos, cdnaStartPos, aaStart, aaEnd):
    """Extra check for this bed file that the total bases covered by exons equals the length
        of the cdna."""
    if (sum(ret.exonSizes) != cdnaEndPos - cdnaStartPos):
        log("ERROR: sum(blockSizes) != cdnaLen: %d != %d for %s:%d-%d" %
            (sum(ret.exonSizes), cdnaEndPos - cdnaStartPos, ret.name, aaStart,aaEnd))
        log("original bed:")
        logBedRecord(original)
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

def findColor(score, chi2val):
    """Return the right color for this region based on the missense score."""
    # this is decipher's color scheme
    if chi2.sf(chi2val,1) > 0.001:
        return "160,160,160"
    if score < 0.2:
        return "253,0,2"
    elif score >= 0.2 and score < 0.4:
        return "233,127,5"
    elif score >= 0.4 and score < 0.6:
        return "224,165,8"
    elif score >= 0.6 and score < 0.8:
        return "127,233,58"
    elif score >= 0.8:
        return "0,244,153"

def txToDict(txFh, verbose=False):
    """Make a dict of all the transcripts for lookup later."""
    lineCount = 1
    for line in txFh:
        splitLine = line.strip().split('\t')
        bed = lineToBed(splitLine, lineCount)
        txDict[splitLine[3]].append(bed)
        lineCount += 1

def aaToBedNegStrand(aaStart, aaEnd, score, transcript, lineNum):
    """The negative strand conversion is slightly different than the more intuitive positive
        strand conversion. Mostly the difference is because you have to walk over the exons
        backwards and swap chromStart and ends. So cdnaStartPos and cdnaEndPos have the
        opposite meaning, cdnaStartPos corresponds to a bed's chromEnd, etc."""
    cdnaStartPos = (aaStart - 1) * 3 # 0-based start
    cdnaEndPos = (aaEnd * 3) # 0-based half open end
    chromStart = None
    chromEnd = None
    cdnaStartExon = 0
    cdnaEndExon = 0
    length = 0
    outStarts = []
    outSizes = []
    for i in range(transcript.exonCount-1,-1,-1):
        exonStartPos = transcript.chromStart + transcript.exonStarts[i]
        exonEndPos = exonStartPos + transcript.exonSizes[i]
        if chromEnd is None and cdnaStartPos < sum(transcript.exonSizes[i:]):
            cdnaStartExon = i
            # subtract a little bit from the right side of the exon block
            # if you were looking at the exon in the browser. the sum
            # takes care of when we start in say the 4th exon out of 12 or something
            chromEnd = transcript.chromStart + transcript.exonStarts[i] + (sum(transcript.exonSizes[i:]) - cdnaStartPos)
            outStarts = transcript.exonStarts[:i+1]
            outSizes = transcript.exonSizes[:i]
            # the size of this exon is the old size minus how many bases into this exon
            # the cndaStartPos is
            outSizes += [transcript.exonSizes[i] - (exonEndPos - chromEnd)]
        if chromStart is None and cdnaEndPos <= sum(transcript.exonSizes[i:]):
            cdnaEndExon = i
            chromStart = transcript.chromStart + transcript.exonStarts[i] + (sum(transcript.exonSizes[i:]) - cdnaEndPos)
            # adjust all the chromStarts according to our new choice
            outStarts = [0] + [x - (chromStart - transcript.chromStart) for x in outStarts[i+1:]]
            if cdnaEndExon != cdnaStartExon:
                outSizes = [transcript.exonSizes[i] - (chromStart - exonStartPos)] + outSizes[i+1:]
            else:
                outSizes = [chromEnd - chromStart] + outSizes[i+1:]

    pscore = score * 1000
    if pscore > 1000:
        pscore = 1000
    ret = bedInfo(transcript.chrom, chromStart, chromEnd, transcript.name, "%d" % int(pscore),
        transcript.strand, chromStart, chromEnd, "255,0,0", cdnaStartExon-cdnaEndExon+1,
        outSizes, outStarts)
    checkAaLenToBlockLen(ret, transcript, cdnaEndPos, cdnaStartPos, aaStart, aaEnd)
    sanityCheckBed12(ret)
    return ret

def aaToBedPosStrand(aaStart, aaEnd, score, transcript, lineNum):
    """Convert an amino acid range like 1:50 to the corresponding genomic position, taking
        into account exon blocks that define the coding sequence."""
    cdnaStartPos = (aaStart - 1) * 3 # 0-based start
    cdnaEndPos = (aaEnd * 3) # 0-based half open end
    chromStart = None
    chromEnd = None
    cdnaStartExon = 0
    cdnaEndExon = 0
    length = 0
    outStarts = []
    outSizes = []
    for i in range(transcript.exonCount):
        exonStartPos = transcript.chromStart + transcript.exonStarts[i]
        exonEndPos = exonStartPos + transcript.exonSizes[i]
        if chromStart is None and cdnaStartPos < sum(transcript.exonSizes[:i+1]):
            cdnaStartExon = i
            chromStart = exonStartPos + (cdnaStartPos - sum(transcript.exonSizes[:i]))
            outStarts.append(0)
            outStarts += [x - (chromStart - transcript.chromStart) for x in transcript.exonStarts[i+1:]]
            outSizes = [transcript.exonSizes[i] - (chromStart - exonStartPos)]
            outSizes += [x for x in transcript.exonSizes[i+1:]]
        if chromEnd is None and cdnaEndPos <= sum(transcript.exonSizes[:i+1]):
            cdnaEndExon = i
            chromEnd = exonStartPos + (cdnaEndPos - sum(transcript.exonSizes[:i]))
            outSizes = outSizes[:cdnaEndExon-cdnaStartExon]
            outSizes += [chromEnd - (chromStart + outStarts[cdnaEndExon-cdnaStartExon])]
            outStarts = outStarts[:cdnaEndExon-cdnaStartExon+1]

    pscore = score * 1000
    if pscore > 1000:
        pscore = 1000
    ret = bedInfo(transcript.chrom, chromStart, chromEnd, transcript.name, "%d" % int(pscore),
        transcript.strand, chromStart, chromEnd, "255,0,0", cdnaEndExon-cdnaStartExon+1,
        outSizes, outStarts)
    checkAaLenToBlockLen(ret, transcript, cdnaEndPos, cdnaStartPos, aaStart, aaEnd)
    sanityCheckBed12(ret, transcript)
    return ret

def aaToBedByStrand(aaStart, aaEnd, score, observed, expected, chi2, gene, transcript, lineNum):
    """Dispatch to write function depending on strand."""
    ret = []
    if transcript.strand == "+":
        ret = aaToBedPosStrand(aaStart, aaEnd, score, transcript, lineNum)
    else:
        ret = aaToBedNegStrand(aaStart, aaEnd, score, transcript, lineNum)
    # now add the extra scoring information
    color = "0,0,0" # findColor(score, chi2)
    ret = ret._replace(itemRgb=color)
    print("%s\t%s\t%d\t%.3f\t%.3f\t%.3f\tO/E: %0.3f" % (bedToStr(ret), gene, observed, expected, score, chi2, score))

def aaToBed(aaFh, verbose):
    """
    Using the transcript dict, get the coordinates for an amino acid range in bed12 format.
    Example input line:
    ENST00000337907.3   RERE    1   1-507   8716356 8424825 97  197.9807    0.489947    51.505535   RERE_1
    Example output line:
    chr1    8424824 8716356 ENST00000337907.3   0.49    -   8424824 8716356 224,165,8   13  74,163,81,99,100,125,49,105,97,106,126,71,325   0,1047,57962,101160,130298,132640,143861,176448,191709,192652,249795,259544,291207 RERE 97 197.98 0.48 51.50
    Example output line:
    """
    lineCount = 1
    for line in aaFh:
        if line.startswith('transcript'):
            continue
        splitLine = line.split('\t')
        tx = splitLine[0]
        gene = splitLine[1]
        chrom = splitLine[2]
        aaRange = splitLine[3]
        observed = int(splitLine[6])
        expected = float(splitLine[7])
        score = float(splitLine[8])
        chi2Val = float(splitLine[9])
        try:
            bedInfo = txDict[tx]
            aaSeq = aaRange.split('-')
            aaStart = int(aaSeq[0])
            aaEnd = int(aaSeq[1])
            for bed in bedInfo: # PAR region transcripts can have more than one
                aaToBedByStrand(aaStart, aaEnd, score, observed, expected, chi2Val, gene, bed, lineCount)
        except ValueError:
            if verbose:
                log("ERROR: transcript '%s' not found in transcripts file, line %d of input aa file" %
                    (tx,lineCount))
        lineCount += 1

def main():
    """open up the input bed12 and the input aa strings and call the converter."""
    args = commandLine()
    verbose = args.verbose
    txFname = args.transcripts
    aaFname = args.aaRanges

    if txFname is not "-":
        with open(txFname) as f:
            txToDict(f)
    else:
        txToDict(sys.stdin)
    if verbose:
        log("%d transcripts added to transcript dict" % len(txDict))

    if aaFname is not "-":
        with open(aaFname) as f:
            aaToBed(f, verbose)
    else:
        aaToBed(sys.stdin, verbose)

if __name__ == "__main__":
    main()
