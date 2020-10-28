#!/usr/bin/env python3

"""Convert gnomAD pext values to one bedGraph per tissue"""

import sys,argparse,os,gzip,math,signal
from collections import namedtuple,defaultdict,OrderedDict

defaultDir = os.getcwd() + "/output"

bed5 = ["chrom", "start", "end", "gene", "score"]
bedRange = namedtuple("bedRange", bed5)

def setupCommandLine():
    parser = argparse.ArgumentParser(description="Convert gnomAD pext values per gene into per tissue bedGraphs")
    parser.add_argument("infile", help="Input optionally gzipped tab separated pext values, the first line of this file must contain all the different tissues. To read from stdin use 'stdin'")
    parser.add_argument("-o", "--outdir", default=defaultDir, help="Output directory for each bedGraph, defaults to './output'")
    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)
    args = parser.parse_args()
    return args

def writeExonScoresToFile(tissueFhs, tissues, tissueRegionScores):
    """Write a per tissue bedGraph line"""
    for (ix,tissue) in enumerate(tissueRegionScores):
        for region in tissueRegionScores[tissue]:
            string = "%s\t%s\t%s\t%0.5f\n" % (region.chrom, region.start, region.end, tissueRegionScores[tissue][region])
            tissueFhs[ix].write(string)

def setupOutput(outdir,line):
    """Set up the output files we will be writing to, and a signal handler for trapping ctrl-c"""
    header = line.strip()
    tissues = header.split('\t')[3:]
    tissueFhs = []
    if not outdir.endswith("/"):
        outdir += "/"
    for t in tissues:
        tissueFh = open(outdir + t + ".bed", "w+")
        tissueFhs.append(tissueFh)
    return outdir,tissues,tissueFhs

def writePreviousRangeScores(tissueRangeScores, tissueList, geneList, tIndex, tissueFhs):
    """For each tissue in tissueList, write a new bedGraph line and delete the entry."""
    if tIndex == -1:
        for ix,tissue in enumerate(tissueRangeScores):
            if geneList is None:
                for gene in tissueRangeScores[tissue]:
                    rangeScore = tissueRangeScores[tissue][gene]
                    tissueFhs[ix].write("%s\t%s\t%s\t%s\t%0.5f\n" % (rangeScore.chrom,rangeScore.start,rangeScore.end, gene, rangeScore.score))
            else:
                for gene in geneList:
                    rangeScore = tissueRangeScores[tissue][gene]
                    tissueFhs[ix].write("%s\t%s\t%s\t%s\t%0.5f\n" % (rangeScore.chrom,rangeScore.start,rangeScore.end, gene, rangeScore.score))
                    
    else:
        if geneList is None:
            for gene in tissueRangeScores[tissueList[0]]:
                rangeScore = tissueRangeScores[tissueList[0]][gene]
                tissueFhs[tIndex].write("%s\t%s\t%s\t%s\t%0.5f\n" % (rangeScore.chrom,rangeScore.start,rangeScore.end, gene, rangeScore.score))
        else:
            for gene in geneList:
                rangeScore = tissueRangeScores[tissueList[0]][gene]
                tissueFhs[tIndex].write("%s\t%s\t%s\t%s\t%0.5f\n" % (rangeScore.chrom,rangeScore.start,rangeScore.end, gene, rangeScore.score))

def addToOrStartNewRange(oldRange, tissueRangeScores, newPosition, vals, tissues, tissueFhs):
    """For all the tissues in vals, figure out if we are still in the same exon
            and adding to the previous range/score combo, or if we are in the same
            exon but with a different score, or a new exon altogether.
       Return: The new range (either an expanded oldRange or brand new range if a new exon)"""
    newRange = oldRange
    if not oldRange:
        newRange = newPosition
        for t,tissue in enumerate(tissueRangeScores):
            tissueRangeScores[tissue][newPosition.gene] = newPosition._replace(score=float(vals[t]))
    else:
        if newPosition.chrom != oldRange.chrom:
            #brand new exon/gene
            writePreviousRangeScores(tissueRangeScores, tissues, None, -1, tissueFhs)
            for t,tissue in enumerate(tissues):
                newBed = newPosition._replace(score=float(vals[t]))
                tissueRangeScores[tissue].clear()
                tissueRangeScores[tissue][newPosition.gene] = newBed
            newRange = newPosition
        else:
            if newPosition.start > oldRange.end:
                # new exon
                writePreviousRangeScores(tissueRangeScores,tissues, None, -1, tissueFhs)
                for t,tissue in enumerate(tissues):
                    newPosition = newPosition._replace(score=float(vals[t]))
                    tissueRangeScores[tissue].clear()
                    tissueRangeScores[tissue][newPosition.gene] = newPosition
                newRange = newPosition
            else:
                # figure out whether to extend old record or maybe add a new one
                newRange = oldRange._replace(end=newPosition.end)
                if newPosition.gene not in oldRange.gene:
                    # new gene that overlaps exons, disallow
                    newRange = oldRange._replace(gene=oldRange.gene+[newPosition.gene])
                    for t,tissue in enumerate(tissues):
                        for gene in oldRange.gene:
                            if gene in tissueRangeScores[tissue]:
                                del tissueRangeScores[tissue][gene]
                elif len(oldRange.gene) == 1:
                    for t,tissue in enumerate(tissues):
                        oldRange = tissueRangeScores[tissue][newPosition.gene]
                        newScore = float(vals[t])
                        if oldRange.score == newScore or (math.isnan(newScore) and math.isnan(oldRange.score)):
                            temp = oldRange._replace(end=newPosition.end)
                            tissueRangeScores[tissue][newPosition.gene] = temp
                        else:
                            # write and clear old for this gene, then add new
                            writePreviousRangeScores(tissueRangeScores, [tissue], [newPosition.gene], -1, tissueFhs)
                            tissueRangeScores[tissue][newPosition.gene] = newPosition._replace(score=newScore)
    if type(newRange.gene) is not list:
        newRange = newRange._replace(gene=[newRange.gene])
    return newRange

def buildPext(inFh, outdir):
    """Find the list of tissues from the first line in the file, then make per tissue bedGraphs"""
    currentRange = None
    currentGene = None
    tissues = []
    tissueFhs = []
    def handler(signum, frame):
        """A helper function to close open files if a ctrl-c is sent"""
        print("Keyboard interrupt: closing open files.")
        for fh in tissueFhs:
            fh.close()
        sys.exit(1)

    # most tissues will have a constant score across a majority of the exon
    # so use this structure to compact the scores down from base level to a region level
    tissueRegionScores = {}
    for line in inFh:
        if "str" not in str(type(line)):
            line = line.decode("ASCII")
        if line.startswith("ensg"):
            outdir, tissues, tissueFhs = setupOutput(outdir, line)
            signal.signal(signal.SIGINT, handler)
            tissueRegionScores = OrderedDict({t : {} for t in tissues})
            continue
        fields = line.strip().split('\t')
        ensg = fields[0]
        pos = fields[1]
        gene = fields[2]
        vals = fields[3:]
        chrom,position = pos.split(':')
        # use ensg as gene because sometimes the same gene has mulitple ensg ids (EFNA3)
        thisRange = bedRange._make(["chr"+chrom,int(position)-1,int(position),ensg,-1])
        currentRange = addToOrStartNewRange(currentRange, tissueRegionScores, thisRange, vals, tissues, tissueFhs)
    # write the final line:
    writePreviousRangeScores(tissueRegionScores, tissueRegionScores.keys(),None,-1,tissueFhs)

def main():
    args = setupCommandLine()
    try:
        os.mkdir(args.outdir)
    except FileExistsError:
        pass
    if args.infile != "stdin":
        if args.infile[-4:] == ".bgz" or args.infile[-3:] == ".gz":
            with gzip.open(args.infile, "rb") as f:
                buildPext(f, args.outdir)
        else:
            with open(args.infile) as f:
                buildPext(f, args.outdir)
    else:
        buildPext(sys.stdin, args.outdir)

if __name__ == "__main__":
    main()
