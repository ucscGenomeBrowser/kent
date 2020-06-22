#!/usr/bin/env python3

import logging, argparse, sys
import random
from collections import defaultdict
from utils import die
import newick, cladeColors, nextstrainVcf, vcf, virusNames

def main():
    parser = argparse.ArgumentParser(description="""
Read in tree, read in samples from VCF, attempt to match tree's leaf IDs with VCF IDs,
prune tree to only branches with leaves found in VCF, output pruned tree with VCF IDs.
"""
    )
    parser.add_argument('treeFile', help='Newick file with IDs similar to Nextstrain')
    parser.add_argument('vcfFile', help='VCF file derived from Nextstrain data')
    args = parser.parse_args()
    # Very large, deeply nested trees can exceed the default recursion limit of 1000.
    sys.setrecursionlimit(100000)
    # logging.basicConfig(level=logging.DEBUG, filename='intersect.log')
    tree = newick.parseFile(args.treeFile)
    (vcfSamples, vcfSampleClades) = nextstrainVcf.readVcfSamples(args.vcfFile)
    idLookup = virusNames.makeIdLookup(vcfSamples)
    for key in idLookup:
        values = idLookup[key]
        if (len(values) != 1):
            logging.warn('Duplicate name/component in VCF: ' + key + " -> " + ", ".join(values))
    sampleSet = set()
    tree = newick.treeIntersectIds(tree, idLookup, sampleSet, virusNames.lookupSeqName)
    cladeColors.addCladesAsBogusLength(tree, vcfSampleClades)
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
