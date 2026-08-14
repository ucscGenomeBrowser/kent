#!/usr/bin/env python3
# Generate the geneticCodes[] C initializer for src/lib/dnautil.c from the
# NCBI genetic code table (gc.prt).
#
# The NCBI ncbieaa / sncbieaa strings are 64 characters in the same codon
# order as dnautil.c's codonTable[] (base1 outer, base3 inner, each TCAG,
# i.e. ttt,ttc,tta,ttg,tct,...), so each genetic code maps index-for-index
# with no reordering.  '*' marks a stop codon; dnautil.c converts that to 0
# at lookup time.  sncbieaa marks alternative start codons with 'M'.
#
# Usage:
#   ./dnaGeneticCodes.py [gc.prt] > ../../lib/geneticCodeTable.h
# (gc.prt is a file arg, or is downloaded from NCBI if omitted).
#
# This writes the auto-generated header src/lib/geneticCodeTable.h, which
# dnautil.c #includes.  Rerun it to refresh the table when NCBI updates gc.prt;
# the struct geneticCode definition and all code live in dnautil.c, so only
# this data header changes.

import re
import sys
import urllib.request

GC_URL = "https://ftp.ncbi.nih.gov/entrez/misc/data/gc.prt"

def loadText(argv):
    if len(argv) > 1:
        with open(argv[1]) as f:
            return f.read()
    with urllib.request.urlopen(GC_URL) as resp:
        return resp.read().decode("ascii")

def parseCodes(text):
    """Return list of (id, name, ncbieaa, sncbieaa) in file order."""
    # Each entry is a {...} block holding one or more name lines, an id, and
    # the two amino acid strings.  Grab each block, then pull fields out of it.
    blocks = re.findall(r"\{\s*(name\s+\"[^{}]*?id\s+\d+[^{}]*?)\}", text, re.DOTALL)
    codes = []
    for b in blocks:
        theId = int(re.search(r"\bid\s+(\d+)", b).group(1))
        name = re.search(r"name\s+\"([^\"]*)\"", b).group(1)
        name = " ".join(name.split())  # collapse wrapped newlines in gc.prt
        ncbieaa = re.search(r"ncbieaa\s+\"([^\"]*)\"", b).group(1)
        sncbieaa = re.search(r"sncbieaa\s+\"([^\"]*)\"", b).group(1)
        for s, what in ((ncbieaa, "ncbieaa"), (sncbieaa, "sncbieaa")):
            if len(s) != 64:
                raise ValueError("code %d %s is %d chars, expected 64"
                                 % (theId, what, len(s)))
        codes.append((theId, name, ncbieaa, sncbieaa))
    return codes

HEADER = """\
/* geneticCodeTable.h - the NCBI genetic codes (translation tables).
 *
 * AUTO-GENERATED from NCBI's gc.prt - do not edit by hand.  To refresh:
 *   src/oneShot/dnaGeneticCodes/dnaGeneticCodes.py > src/lib/geneticCodeTable.h
 *
 * Included by dnautil.c, which defines struct geneticCode.  Each aa/starts
 * string is 64 chars in codonTable[] order; in aa '*' is a stop codon, in
 * starts 'M' is an alternative start codon. */

"""

def emitC(codes, out):
    out.write(HEADER)
    out.write("static struct geneticCode geneticCodes[] = {\n")
    for theId, name, ncbieaa, sncbieaa in codes:
        out.write('    {%2d, "%s",\n' % (theId, name))
        out.write('        "%s",\n' % ncbieaa)
        out.write('        "%s"},\n' % sncbieaa)
    out.write("};\n")

def main():
    codes = parseCodes(loadText(sys.argv))
    if not codes:
        sys.exit("no genetic codes parsed - has the gc.prt format changed?")
    emitC(codes, sys.stdout)

if __name__ == "__main__":
    main()
