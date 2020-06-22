# odds and ends

import sys, logging

def die(message):
    """good old perl die"""
    logging.error(message)
    sys.exit(1)

def listFromFile(fileName):
    """Read in a file; return a list of its lines.  Ignore blank lines."""
    lines = []
    with open(fileName, 'r') as inF:
        line = inF.readline()
        while (line):
            line = line.rstrip()
            if (len(line)):
                lines.append(line)
            line = inF.readline()
    return lines

def dictFromFile(fileName):
    """Read in a tab-separated file with two columns; return a dict mapping first col to second.
    Ignore blank lines."""
    mapper = {}
    with open(fileName, 'r') as inF:
        line = inF.readline()
        while (line):
            line = line.rstrip()
            if (len(line)):
                cols = line.split('\t')
                if (len(cols) < 2):
                    die("Expected at least two columns in file " + fileName + ", but got this:\n" +
                        line + "\n")
                mapper[cols[0]] = cols[1]
            line = inF.readline()
    return mapper
