#!/usr/bin/env python3

import logging, argparse, sys
import random
import newick, utils, virusNames

def main():
    parser = argparse.ArgumentParser(description="""
Read in tree, read in samples from sampleFile, attempt to match tree's leaf IDs with samples,
prune tree to only branches with leaves found in sampleFile, output pruned tree with sample IDs.
"""
    )
    parser.add_argument('treeFile', help='Newick file with IDs similar to Nextstrain')
    parser.add_argument('sampleFile', help='File with one sample ID per line')
    args = parser.parse_args()
    # Very large, deeply nested trees can exceed the default recursion limit of 1000.
    sys.setrecursionlimit(100000)
    # logging.basicConfig(level=logging.DEBUG, filename='debug.log')
    tree = newick.parseFile(args.treeFile)
    samples = utils.listFromFile(args.sampleFile)
    idLookup = virusNames.makeIdLookup(samples)
    for key in idLookup:
        values = idLookup[key]
        if (len(values) != 1):
            logging.warn('Duplicate name/component in ' + args.sampleFile + ': ' + key + " -> " +
                         ", ".join(values))
    foundSampleSet = set()
    tree = newick.treeIntersectIds(tree, idLookup, foundSampleSet, virusNames.lookupSeqName)
    newick.printTree(tree)
    if (len(foundSampleSet) < len(samples)):
        logging.warn("%s has %d samples but pruned tree has %d leaves (%d samples not found)" %
                     (args.sampleFile, len(samples), len(foundSampleSet),
                      len(samples) - len(foundSampleSet)))
        allSampleSet = set(samples)
        sampleFileNotTree = allSampleSet - foundSampleSet
        logging.warn("Example samples not found:\n" +
                     "\n".join(random.sample(sampleFileNotTree, 10)))

main()
