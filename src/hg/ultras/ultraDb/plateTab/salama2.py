#!/usr/bin/env python

import sys

def reformat(inName, outName):
    """Transform spreadsheet format into database format"""
    print "Processing %s to %s" % (inName, outName)
    inFile = open(inName)
    outFile = open(outName, "w")
    while 1:
        line = inFile.readline()
	if line == '':
	    break
	(rowCol, row, col, name, seq, seqSize, prodSize, purpose) = line.split('\t')
	purpose = purpose[:-1]
	outFile.write(name + '\t')
	outFile.write(name + '\t')
	outFile.write(seq + '\t')
	outFile.write("Salama2" + '\t')
	outFile.write(row + '\t')
	outFile.write(col + '\t')
	outFile.write("entry" + '\t')
	outFile.write(purpose + '\t')
	outFile.write(prodSize + '\n')

if len(sys.argv) != 3:
    sys.exit("Usage: salama2.py inName.txt outName.txt")
reformat(sys.argv[1], sys.argv[2])

