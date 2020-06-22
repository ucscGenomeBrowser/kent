#!/usr/bin/env python3

import logging, argparse, sys
import newick, utils

def countSampleLeaves(node, samples):
    """Count the number of leaves descending from this node whose labels are in samples.
    Add the number as node['sampleCount'] and also return the list of samples found."""
    samplesFound = set()
    if (node['kids']):
        for kid in node['kids']:
            samplesFound = samplesFound.union(countSampleLeaves(kid, samples))
    else:
        if (node['label'] in samples):
            samplesFound = set([node['label']])
    node['sampleCount'] = len(samplesFound)
    return samplesFound

def topBranchWithSampleCount(node, sampleCount):
    """If a descendant of this node is the topmost branch with sampleCount then return the
    descendant.  Return self if no descendant is the topmost branch but this node has sampleCount."""
    if (node['sampleCount'] < sampleCount):
        return None;
    if (node['kids']):
        for kid in node['kids']:
            topKid = topBranchWithSampleCount(kid, sampleCount)
            if (topKid):
                return topKid
    return node

def samplesMissing(samples, samplesFound):
    """Return a string with a limited number of samples not found for error reporting."""
    missing = list(set(samples) - samplesFound)
    if (len(missing) > 10):
        return ', '.join(missing[0:10]) + ', ...'
    else:
        return ', '.join(missing)

def treeBranchWithSamples(tree, samples):
    """Return the topmost (farthest from root) branch that has all samples as descendants.
    Error out if not all samples are found."""
    sampleSet = set(samples)
    if (len(samples) > len(sampleSet)):
        utils.die("List of samples must be unique, but contains duplicate(s)")
    samplesFound = countSampleLeaves(tree, sampleSet)
    if (len(samplesFound) != len(sampleSet)):
        utils.die("Expected to find %d samples, but found %d.  (Not found: %s)" %
                  (len(sampleSet), len(samplesFound), samplesMissing(sampleSet, samplesFound)))
    return topBranchWithSampleCount(tree, len(samplesFound))

def main():
    parser = argparse.ArgumentParser(description="""
Read in tree, read samples, find branch of tree that has all of the samples as leaves,
and write out that branch as a new tree.  All samples must exactly match leaf names
and all must be found.
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
    branch = treeBranchWithSamples(tree, samples)
    newick.printTree(branch)

main()
