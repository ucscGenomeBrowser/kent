#!/bin/env python3

"""
Filter top N tracks from a cooccurrence matrix.

usage: filterTop.py -n 100 -t trackName inFile outFile

n defaults to 25
"""

import sys,argparse,math,gzip

# for quickly reading text files into int arrays
import numpy as np


trackNames = [] # ["knownGene", "snp150", ... ]
matrix = [] # will become numpy array of len(trackNames) x len(trackNames)
offsets = [] # offsets of specific rows into input matrix

hg19DefaultTracks = ["hg19Patch13Haplotypes","hg19Patch13Patches","hg19Patch13","cytoBandIdeo","knownGene","ncbiRefSeq","ncbiRefSeqCurated","ncbiRefSeqOther","ncbiRefSeqPsl","refGene","ncbiRefSeqHgmd","refSeqComposite","pubsBlat","pubsMarkerSnp","pubs","gtexGene","wgEncodeRegMarkH3k27acGm12878","wgEncodeRegMarkH3k27acH1hesc","wgEncodeRegMarkH3k27acHsmm","wgEncodeRegMarkH3k27acHuvec","wgEncodeRegMarkH3k27acK562","wgEncodeRegMarkH3k27acNhek","wgEncodeRegMarkH3k27acNhlf","wgEncodeRegMarkH3k27ac","wgEncodeRegDnaseClustered","wgEncodeRegTfbsClusteredV3","phyloP100wayAll","multiz100way","cons100way","snp151Common","rmsk"]

class CommandLine():
    """Parse command line, handle program usage, and help"""
    def __init__(self, inOpts=None):
        self.parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
            description = "Filter top N tracks from a cooccurrence matrix..", add_help=True,
            prefix_chars = "-", usage = "%(prog)s [options]",
            epilog="Examples:\nSee the top 25 (the default choice) tracks that are used with\ndgvMerged from a single error log, but exclude any tracks that\nare on by default for hg19:\n    cooccurrenceCounts -db=hg19 hgw4.error_log.20180401.gz stdout | \\\n        cut -f2- | ./filterTop.py -t dgvMerged --remove-defaults stdin stdout | \\\n        head -1\n\nSee the top 100 tracks that are used from a single error log:\n    cooccurrenceCounts -db=hg19 hgw4.error_log.20180401.gz stdout | \\\n        cut -f2- | ./filterTop.py -n 100 --remove-defaults stdin top25.tsv\n\nNote the \"cut -f2-\" in the examples, this is because currently the\ncooccurrenceCounts program output a row label, which this program does not like.\n")
        self.parser.add_argument('-n', type=int, default=25,
            help="Number of related tracks to keep, defaults to 25.")
        self.parser.add_argument('-t', '--track', action='store',
            help="Show -n size matrix (default 25) for this track only.")
        self.parser.add_argument('--remove-defaults', action='store_true', default=False,
            help="Remove default tracks. If -t option is used then that track will be included but others will be removed. Currently hardcoded to the hg19 default track set.")
        self.parser.add_argument('inFile', action='store',
            help="Input cooccurrence matrix file to parse, use \"stdin\" to read from stdin.")
        self.parser.add_argument('outFile', action='store',
            help="Output file name, use \"stdout\" to write to stdout.")

        if inOpts is None:
            self.args = self.parser.parse_args()
        else:
            self.args = self.parser.parse_args(inOpts)

        if len(sys.argv) == 1:
            self.parser.print_help()

        if not self.args.inFile:
            print("Missing inFile.")
            self.parser.print_help()
            sys.exit(1)

def writeMatrix(outFh, mat):
    """pretty print matrix to out file"""
    outFh.write("\t".join(trackNames))
    outFh.write("\n")
    for row in mat:
        ret = ""
        for col in row:
            ret += "%s\t" % col
        ret += "\n"
        outFh.write(ret)

def loadMatrix(inFh):
    """Get labels and then load matrix into memory."""
    global matrix
    global trackNames
    lineIx = 0
    numColumns = 0
    for line in inFh:
        if "str" not in str(type(line)):
            line = line.decode("ASCII")
        if lineIx == 0:
            l = line.strip().split('\t')
            if l[0] == "tracks" or l[0][0] == "#": # first line
                if l[0] == "tracks":
                    trackNames =  l[1:]
                elif l[0][0] == "#":
                    trackName = l[0][1:]
                    if trackName[0] != "":
                        trackNames += l[1:]
                    else:
                        trackNames = l[1:]
            else:
                trackNames = l
            trackNames = np.array(trackNames, dtype=np.str_)
            lineIx += 1
            numColumns = len(trackNames)
            #sys.stderr.write("numColumns = %d\n" % numColumns)
            matrix = np.zeros((numColumns, numColumns))
        else:
            matrix[lineIx-1] = np.fromstring(line, dtype=np.int64, count=numColumns, sep='\t')
            lineIx += 1

def filterOneTrack(inFh, cutoff, tName, removeDefaults=False):
    """For a given tName, make a matrix of the top cutoff tracks used
       along with it."""
    global matrix
    global trackNames
    global hg19DefaultTracks
    if tName in hg19DefaultTracks:
        hg19DefaultTracks.remove(tName)
    tIxTuple = np.where(trackNames == tName) # returns a tuple ([tIx],None)
    tIx = tIxTuple[0][0] # get first elem of tuple, first elem of array
    sortIx = []
    if removeDefaults:
        temp = np.argsort(matrix[tIx])[::-1]
        nonDefaults = list(set(trackNames) - set(hg19DefaultTracks))
        sortIx = [index for index in temp if trackNames[index] in nonDefaults]
        sortIx = np.array(sortIx)[:cutoff]
    else:
        sortIx = np.argsort(matrix[tIx])[::-1][:cutoff] # top cutoff track indexes used with tName
    trackNames = trackNames[sortIx]
    return matrix[np.ix_(sortIx, sortIx)].astype(int)

def filterTopTracks(inFh, cutoff, removeDefaults=False):
    """Move along diagonal of matrix and collect tracks counts.
       Then sort and find the cutoff number of tracks.
       Lastly go back through the matrix and make a new matrix
       of these cutoff number of tracks."""
    global matrix
    global trackNames
    diag = np.diagonal(matrix).copy()
    sortIx = []
    if removeDefaults:
        temp = np.argsort(diag)[::-1]
        nonDefaults = list(set(trackNames) - set(hg19DefaultTracks))
        sortIx = [index for index in temp if trackNames[index] in nonDefaults]
        sortIx = np.array(sortIx)[:cutoff]
    else:
        sortIx = np.argsort(diag)[::-1][:cutoff] # indexes of tracks with counts above cutoff
    trackNames = trackNames[sortIx]
    return matrix[np.ix_(sortIx,sortIx)].astype(int)

def main(args=None):
    """Sets up command line processing and does input processing"""
    if args is None:
        args = CommandLine().args
    else:
        args = CommandLine(args).args

    removeDefaults = args.remove_defaults
    inFh = ""
    #sys.stderr.write("parsing input file: %s\n"% args.inFile)
    if args.inFile[-3:] == ".gz":
        inFh = gzip.open(args.inFile, "rb")
    else:
        if args.inFile == "stdin":
            inFh = sys.stdin
        else:
            inFh = open(args.inFile, "rb")

    loadMatrix(inFh) # also computes trackNames
    if args.track is not None:
        cutoff = args.n if args.n < len(trackNames) else len(trackNames)
        ret = filterOneTrack(inFh, cutoff, args.track, removeDefaults)
        if (len(ret) > cutoff):
            sys.stderr.write("error in filterOneTrack, len(ret) = %d, cutoff = %d\n" % (len(ret), args.n))
            sys.exit(1)
    else:
        ret = filterTopTracks(inFh, args.n, removeDefaults)
    inFh.close()

    outFh = ""
    if args.outFile == "stdout":
        outFh  = sys.stdout
    else:
        outFh = open(args.outFile, "w")

    writeMatrix(outFh, ret)
    if args.outFile != "stdout":
        outFh.close()

if __name__ == '__main__':
    main()
