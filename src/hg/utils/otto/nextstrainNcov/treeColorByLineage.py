#!/usr/bin/env python3

import logging, argparse, sys
from collections import defaultdict
import newick, lineageColors, utils, virusNames

def main():
    parser = argparse.ArgumentParser(description="""
Read in tree, read in samples and lineages, attempt to match tree's leaf IDs with lineage IDs,
add colors corresponding to lineages.
"""
    )
    parser.add_argument('treeFile', help='Newick file with IDs similar to Nextstrain')
    parser.add_argument('lineageFile', help='Two-column tab-sep file mapping sample to lineage')
    args = parser.parse_args()
    # Very large, deeply nested trees can exceed the default recursion limit of 1000.
    sys.setrecursionlimit(100000)
    tree = newick.parseFile(args.treeFile)
    sampleLineages = utils.dictFromFile(args.lineageFile)
    treeNames = newick.leafNames(tree)
    idLookup = virusNames.makeIdLookup(treeNames)
    treeLineages = dict([ (virusNames.maybeLookupSeqName(name, idLookup), lin)
                          for name, lin in sampleLineages.items() ])
    noLinCount = lineageColors.addLineagesAsBogusLength(tree, treeLineages)
    if (noLinCount):
        logging.warn("%d samples had no lineage in %s" % (noLinCount, args.lineageFile))
    newick.printTree(tree)

main()

