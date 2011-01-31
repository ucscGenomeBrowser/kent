#!/usr/bin/env python

from optparse import OptionParser
import os
import random
import re
import sys

def printUsage():
    print "checkRandomLinesExist.py: sample the source file for a random number of", \
          "lines, and search for those lines in the destination file"
    print "usage:"
    print "   checkRandomLinesExist.py -s <sourceFile> -d <destFile. [-n <nLines>]"
    print "-s <file>   Pathname of the source file"
    print "-d <file>   Pathname of the destination file"
    print "-n <nLines> Number of lines to sample (default: 100)"

parser = OptionParser()
parser.add_option("-n", "--numberLinesToSample", dest="numberLinesToSample",
                  default=100)
parser.add_option("-s", "--sourceFile", dest="sourceFile", default="")
parser.add_option("-d", "--destinationFile", dest="destinationFile", default="")
(parameters, args) = parser.parse_args()

if parameters.sourceFile == "" or parameters.destinationFile == "":
    printUsage()
else:
    fileToSample = parameters.sourceFile
    fileToCompareTo = parameters.destinationFile

    randomlyOrderedLines = list(open(fileToSample))
    random.shuffle(randomlyOrderedLines)
    linesRead = len(randomlyOrderedLines)
    linesToRead = int(parameters.numberLinesToSample)
    if (linesRead > linesToRead) :
        randomlyOrderedLines = randomlyOrderedLines[0:linesToRead]
    for line in randomlyOrderedLines:
        line = line.rstrip()
        grepCmd = "grep \"" + line  + "\" " + fileToCompareTo
        linesReturned = list(os.popen(grepCmd))
        if (len(linesReturned) == 0):
            print "Missing in ", fileToCompareTo, ": ", line
