#!/usr/bin/env python3

import logging, argparse, sys
import random
from utils import die
import newick, vcf, virusNames

def main():
    parser = argparse.ArgumentParser(description="""
Read in tree, read in samples from VCF, attempt to match tree's leaf IDs with VCF IDs,
prune tree to only branches with leaves found in VCF, output pruned tree with VCF IDs.
"""
    )
    parser.add_argument('treeFile', help='Newick file with IDs similar to Nextstrain')
    parser.add_argument('vcfFile', help='VCF file with IDs similar to Nextstrain')
    args = parser.parse_args()
    # Very large, deeply nested trees can exceed the default recursion limit of 1000.
    sys.setrecursionlimit(100000)
    # logging.basicConfig(level=logging.DEBUG, filename='intersect.log')
    tree = newick.parseFile(args.treeFile)
    vcfSamples = vcf.readSamples(args.vcfFile)
    idLookup = virusNames.makeIdLookup(vcfSamples)
    badKeys = []
    for key in idLookup:
        values = idLookup[key]
        if (len(values) > 3):
            badKeys.append(key)
        elif (len(values) != 1):
            logging.warn('Duplicate name/component in VCF: ' + key + " -> " + ", ".join(values))
    for key in badKeys:
        del idLookup[key]
    sampleSet = set()
    tree = newick.treeIntersectIds(tree, idLookup, sampleSet, virusNames.lookupSeqName)
    newick.printTree(tree)
    if (len(sampleSet) < len(vcfSamples)):
        logging.warn("VCF has %d samples but pruned tree has %d leaves (%d VCF samples not found)" %
                     (len(vcfSamples), len(sampleSet), len(vcfSamples) - len(sampleSet)))
        vcfSampleSet = set(vcfSamples)
        vcfNotTree = vcfSampleSet - sampleSet
        logging.warn("Example VCF samples not found:\n" +
                     "\n".join(random.sample(vcfNotTree, 10)))
        vcfOutName = 'intersected.vcf'
        logging.warn("Writing VCF to " + vcfOutName)
        vcf.pruneToSamples(args.vcfFile, sampleSet, vcfOutName)

main()
