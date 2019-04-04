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
defaultTrackList = [] # list of tracks to filter out, provided by user

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
        self.parser.add_argument('--default-track-list', action='store',
            help="List of tracks (ie: default tracks) to remove from results, one track per line.")
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
            print("ERROR: Missing inFile.")
            self.parser.print_help()
            sys.exit(1)

        if self.args.default_track_list:
            filterFh = open(self.args.default_track_list)
            setDefaultTracks(filterFh)

def setDefaultTracks(inFh):
    """Builds up the defaultTrackList global"""
    for line in inFh:
        track = line.strip()
        defaultTrackList.append(track)

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
            splitLine = line.strip().split('\t')
            # first line
            if splitLine[0] == "tracks" or splitLine[0] == "#tracks" or splitLine[0] == "#":
                trackNames =  splitLine[1:]
            elif splitLine[0][0] == "#" and splitLine[0] != "#tracks":
                # skip the leading "#"
                firstTrack = splitLine[0][1:]
                trackNames = [firstTrack] + splitLine[1:]
            else:
                trackNames = splitLine
            trackNames = np.array(trackNames, dtype=np.str_)
            lineIx += 1
            numColumns = len(trackNames)
            matrix = np.zeros((numColumns, numColumns))
        else:
            matrix[lineIx-1] = np.fromstring(line, dtype=np.int64, count=numColumns, sep='\t')
            lineIx += 1

def filterOneTrack(inFh, cutoff, tName, removeDefaults=False):
    """For a given tName, slice out a matrix of the top cutoff tracks used
       along with it."""
    if tName in defaultTrackList:
        defaultTrackList.remove(tName)
    # returns a tuple ([tIx],None)
    tIxTuple = np.where(trackNames == tName)
    # get first elem of tuple, first elem of array
    tIx = tIxTuple[0][0]
    matrixIndices = np.argsort(matrix[tIx])[::-1]
    return sliceMatrix(matrixIndices, cutoff, removeDefaults)

def filterTopTracks(inFh, cutoff, removeDefaults=False):
    """Move along diagonal of matrix and collect tracks counts.
       Then slice the matrix, according to the cutoff value."""
    diagIndices = np.argsort(np.diagonal(matrix).copy())[::-1]
    return sliceMatrix(diagIndices, cutoff, removeDefaults)

def sliceMatrix(matrixIndices, cutoff, removeDefaults=False):
    """For a given matrix, slice out the region we are interested in"""
    global trackNames
    if removeDefaults:
        nonDefaults = list(set(trackNames) - set(defaultTrackList))
        indices = [index for index in matrixIndices if trackNames[index] in nonDefaults]
        sortedCutoffIndices = np.array(indices)[:cutoff]
    else:
        sortedCutoffIndices = matrixIndices[:cutoff]
    trackNames = trackNames[sortedCutoffIndices]
    return matrix[np.ix_(sortedCutoffIndices,sortedCutoffIndices)].astype(int)

def main(args=None):
    """Sets up command line processing and does input processing"""
    if args is None:
        args = CommandLine().args
    else:
        args = CommandLine(args).args

    removeDefaults = False
    if args.default_track_list is not None:
        removeDefaults = True
    inFh = ""
    #sys.stderr.write("parsing input file: %s\n"% args.inFile)
    if args.inFile.endswith(".gz"):
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
            sys.stderr.write("ERROR: filterOneTrack: len(ret) = %d, cutoff = %d\n" %
                (len(ret), args.n))
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
