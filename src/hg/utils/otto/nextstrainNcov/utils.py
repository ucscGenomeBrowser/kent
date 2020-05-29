# odds and ends

import sys, logging

def die(message):
    """good old perl die"""
    logging.error(message)
    sys.exit(1)

def dictFromFile(fileName):
    """Read in a tab-separated file with two columns; return a dict mapping first col to second.
    Ignore blank lines."""
    mapper = {}
    with open(fileName, 'r') as inF:
        line = inF.readline().rstrip()
        while (line):
            if (len(line)):
                cols = line.split('\t')
                if (len(cols) < 2):
                    die("Expected at least two columns in file " + fileName + ", but got this:\n" +
                        line + "\n")
                mapper[cols[0]] = cols[1]
            line = inF.readline().rstrip()
        inF.close()
    return mapper
