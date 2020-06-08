#!/usr/bin/env python3

import logging, argparse, sys
import random
from collections import defaultdict
from utils import die
import newick, cladeColors, nextstrainVcf, virusNames

def treeIntersectIds(node, idLookup, sampleSet):
    """For each leaf in node, attempt to look up its label in idLookup; replace if found.
    Prune nodes with no matching leaves.  Store new leaf labels in sampleSet."""
    if (node['kids']):
        # Internal node: prune
        prunedKids = []
        for kid in (node['kids']):
            kidIntersected = treeIntersectIds(kid, idLookup, sampleSet)
            if (kidIntersected):
                prunedKids.append(kidIntersected)
        if (len(prunedKids) > 1):
            node['kids'] = prunedKids
        elif (len(prunedKids) == 1):
            node = prunedKids[0]
        else:
            node = None
    else:
        # Leaf: lookup, prune if not found
        label = node['label']
        matchList = virusNames.lookupSeqName(node['label'], idLookup)
        if (not matchList):
            logging.info("No match for leaf '" + label + "'")
            node = None
        else:
            if (len(matchList) != 1):
                logging.warn("Non-unique match for leaf '" + label + "': ['" +
                             "', '".join(matchList) + "']")
            else:
                logging.debug(label + ' --> ' + matchList[0]);
            node['label'] = matchList[0]
            sampleSet.add(matchList[0])
    return node

def vcfSubset(vcfIn, sampleSet, vcfOut):
    """Copy data in vcfIn to vcfOut, but with only the samples in sampleSet."""
    with open(vcfIn, 'r') as inF:
        with open(vcfOut, 'w') as outF:
            line = inF.readline().rstrip()
            while (line):
                if (line.startswith('#CHROM')):
                    colNamesIn = line.split('\t')
                    colNamesOut = colNamesIn[0:9]
                    columnsKept = [True for i in range(0, 9)]
                    for vcfSample in colNamesIn[9:]:
                        if (vcfSample in sampleSet):
                            colNamesOut.append(vcfSample)
                            columnsKept.append(True)
                        else:
                            columnsKept.append(False)
                    outF.write('\t'.join(colNamesOut) + '\n');
                elif (line[0] == '#'):
                    outF.write(line + '\n');
                else:
                    columnsIn = line.split('\t')
                    columnsOut = []
                    for ix, colVal in enumerate(columnsIn):
                        if (columnsKept[ix]):
                            columnsOut.append(colVal)
                    outF.write('\t'.join(columnsOut) + '\n')
                line = inF.readline().rstrip()
            inF.close()
            outF.close()

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
    tree = treeIntersectIds(tree, idLookup, sampleSet)
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
        vcfSubset(args.vcfFile, sampleSet, vcfOutName)

main()
