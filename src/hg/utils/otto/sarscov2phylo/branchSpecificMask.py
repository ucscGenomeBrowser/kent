#!/usr/bin/env python3

import yaml
import re
import subprocess, tempfile
import logging, argparse, os, sys

def getArgs():
    parser = argparse.ArgumentParser(description="""
Apply branch-specific masking to selected nodes as specified in spec.yaml
to in.pb and write out.pb.  Requires matUtils from usher package.
"""
)
    parser.add_argument('pbIn', metavar='in.pb',
                        help='MAT protobuf file for input tree')
    parser.add_argument('yamlIn', metavar='spec.yaml',
                        help='YAML spec that identifies representative node and sites to mask')
    parser.add_argument('pbOut', metavar='out.pb',
                        help='MAT protobuf file to which output tree will be written')
    args = parser.parse_args()
    return args

def die(message):
    """Log an error message and exit with nonzero status"""
    logging.error(message)
    exit(1)

def getSpec(yamlIn):
    """Read yamlIn and make sure it looks like a spec should"""
    with open(yamlIn) as f:
        try:
            spec = yaml.safe_load(f)
        except yaml.YAMLError as e:
            die(e)
    myVersion = 0
    specVersion = spec.get('version')
    if specVersion is None:
        die(f'Expecting to find version {myVersion} in {yamlIn} but no version found.')
    elif specVersion > myVersion:
        die(f'Version {specVersion} in {yamlIn} is too new, this script is at version {myVersion}')
    del spec['version']
    for branch in spec:
        branchSpec = spec[branch]
        repSeq = branchSpec.get('representative')
        if repSeq is None:
            die(f'No representative sequence was given for {branch} in {yamlIn}')
        ranges = branchSpec.get('ranges')
        sites = branchSpec.get('sites')
        reversions = branchSpec.get('reversions')
        if not ranges and not sites and not reversions:
            die('Found none of {ranges, sites, reversions} ' + f'for {branch} in {yamlIn}')
        if ranges:
            for r in ranges:
                try:
                    range(r[0], r[1]+1)
                except TypeError as e:
                    die(f'Unexpected non-list value "{r}" in ranges for {branch} in {yamlIn}')
    return spec

def run(cmd):
    """Run a command and exit with error output if it fails"""
    try:
        subprocess.run(cmd).check_returncode()
    except subprocess.CalledProcessError as e:
        die(e)

def getBacktrack(spec, rep):
    """If spec for branch whose representative is rep has representativeBacktrack, return that
    value, otherwise return 0."""
    for branch in spec:
        branchSpec = spec[branch]
        if branchSpec['representative'] == rep:
            backtrack = branchSpec.get('representativeBacktrack')
            if backtrack is not None:
                return backtrack
    return 0

def getRepresentativeNodes(pbIn, spec):
    """Run matUtils extract --sample-paths on pbIn and find path to each branch's representative.
    Return dict mapping representative name to final node in path."""
    repNodes = {}
    for branch in spec:
        rep = spec[branch]['representative']
        repNodes[rep] = ''
        exclusions = spec[branch].get('exclusions', [])
        for ex in exclusions:
            rep = ex['representative']
            repNodes[rep] = ''
    samplePaths = tempfile.NamedTemporaryFile(delete=False)
    samplePaths.close()
    # matUtils SEGVs if given a full path output file name unless output dir is '/', need to fix that
    run(['matUtils', 'extract', '-i', pbIn, '-d', '/', '--sample-paths', samplePaths.name])
    with open(samplePaths.name) as f:
        for line in f:
            try:
                [fullName, path] = line.rstrip().split('\t')
            except ValueError as e:
                continue
            name = fullName.split('|')[0]
            if repNodes.get(name) is not None:
                nodes = path.split(' ')
                # In most cases we want the last node in the path [-1]
                nodeIx = -1
                # ... but if the last word starts with the sample name (with private mutations)
                # then we do not want to mask just that sample, so backtrack to [-2]
                if nodes[-1].startswith(name):
                    nodeIx = nodeIx - 1
                # ... and the spec might say to backtrack even more (e.g. parent or grandparent):
                nodeIx = nodeIx - getBacktrack(spec, name)
                nodeMuts = nodes[nodeIx]
                # Strip to just the node ID, discard mutations
                node = nodeMuts.split(':')[0]
                repNodes[name] = node;
    # Make sure we found all of them
    for rep in repNodes:
        if repNodes[rep] == '':
            die(f"sample-paths file {samplePaths.name} does not have name {rep}")
    os.unlink(samplePaths.name)
    return repNodes

def getRevPos(rev):
    """Parse the position out of a reversion [ACGT]([0-9]+)[ACGT], return int."""
    m = re.match(r"^[ACGT]([0-9]+)[ACGT]$", rev)
    if not m:
        die(f"expected reversion string but got {rev}")
    return int(m.group(1))

def getExcludedNodes(exclusions, pos, repNodes):
    """If pos is found in exclusions, return a semicolon-separated string of node IDs for pos."""
    excludedNodes = []
    for ex in exclusions:
        if pos == ex['site']:
            rep = ex['representative']
            excludedNodes.append(repNodes[rep])
    return ";".join(excludedNodes)

def makeMaskFile(spec, repNodes, maskFileName):
    """Create a file to use as input for matUtils mask --mask-mutations,
    generated from spec with node IDs from repNodes."""
    with open(maskFileName, 'w') as maskFile:
        for branch in spec:
            branchSpec = spec[branch]
            rep = branchSpec['representative']
            nodeId = repNodes[rep]
            ranges = branchSpec.get('ranges')
            exclusions = branchSpec.get('exclusions', [])
            if ranges:
                for r in ranges:
                    for pos in range(r[0], r[1]+1):
                        excludedNodes = getExcludedNodes(exclusions, pos, repNodes)
                        maskFile.write(f'N{pos}N\t{nodeId}\t{excludedNodes}\n')
            sites = branchSpec.get('sites')
            if sites:
                for pos in sites:
                    excludedNodes = getExcludedNodes(exclusions, pos, repNodes)
                    maskFile.write(f'N{pos}N\t{nodeId}\t{excludedNodes}\n')
            reversions = branchSpec.get('reversions')
            if reversions:
                for rev in reversions:
                    pos = getRevPos(rev)
                    excludedNodes = getExcludedNodes(exclusions, pos, repNodes)
                    maskFile.write(f'{rev}\t{nodeId}\t{excludedNodes}\n')

def main():
    args = getArgs()
    spec = getSpec(args.yamlIn)
    repNodes = getRepresentativeNodes(args.pbIn, spec)
    maskFileName = args.pbIn + '.branchSpecificMask.tsv'
    makeMaskFile(spec, repNodes, maskFileName)
    run(['matUtils', 'mask', '-i', args.pbIn, '--mask-mutations', maskFileName, '-o', args.pbOut])

if __name__ == '__main__':
    main()
