#!/usr/bin/env python3

import json
import re
from warnings import warn

with open('ncov.json') as f:
    ncov = json.load(f)

chrom = 'NC_045512v2'

# Genes (to which AA mutations are relative):
genePos = {}
geneBed = []
for gene,anno in ncov['meta']['genome_annotations'].items():
    if (gene != 'nuc'):
        genePos[gene] = anno['start'] - 1
        geneBed.append([chrom, anno['start']-1, anno['end'], gene])
def bedStart(bed):
    return bed[1]
geneBed.sort(key=bedStart)
with open('nextstrainGene.bed', 'w') as outG:
    for bed in geneBed:
        outG.write('\t'.join(map(str, bed)) + '\n')
    outG.close()

# Variants and "clades"

snvRe = re.compile('^([ACGT])([0-9]+)([ACGT])$')
snvAaRe = re.compile('^([A-Z*])([0-9]+)([A-Z*])$')

clades = {}
variantCounts = {}
variantAaChanges = {}
samples = []

cladeColors = { 'A1a': '73,75,225', 'A2': '75,131,233', 'A2a': '92,173,207',
                'A3': '119,199,164', 'A6': '154,212,122', 'A7': '173,189,81',
                'B': '233,205,74', 'B1': '255,176,65', 'B2': '255,122,53',
                'B4': '249,53,41' }

def cladeColorFromName(cladeName):
    color = cladeColors.get(cladeName);
    if (not color):
        color = 'purple'
    return color

def subtractStart(coord, start):
    return coord - start

def cladeFromVariants(name, variants):
    """Extract bed12 info from an object whose keys are SNV variant names"""
    clade = {}
    snvEnds = []
    varNames = []
    for varName in variants:
        m = snvRe.match(varName)
        if (m):
            snvEnds.append(int(m.group(2)))
            varNames.append(varName)
    if snvEnds:
        snvEnds.sort()
        snvStarts = list(map(lambda x: x-1, snvEnds))
        snvSizes = list(map(lambda x: 1, snvEnds))
        clade['thickStart'] = min(snvStarts)
        clade['thickEnd'] = max(snvEnds)
        clade['name'] = name
        clade['color'] = cladeColorFromName(name)
        clade['varSizes'] = snvSizes
        clade['varStarts'] = snvStarts
        clade['varNames'] = varNames
    return clade

def rUnpackNextstrainTree(branch, parentVariants):
    """Recursively descend ncov.tree and build data structures for genome browser tracks"""
    # Inherit parent variants
    branchVariants = parentVariants.copy()
    # Add variants specific to this branch (if any)
    try:
        # Nucleotide variants specific to this branch
        for varName in branch['branch_attrs']['mutations']['nuc']:
            if (snvRe.match(varName)):
                branchVariants[varName] = 1
                if (not variantCounts.get(varName)):
                    variantCounts[varName] = 0;
        # Amino acid variants specific to this branch
        for geneName in branch['branch_attrs']['mutations'].keys():
            if (geneName != 'nuc'):
                for change in branch['branch_attrs']['mutations'][geneName]:
                    # Get nucleotide coords & figure out which nuc var this aa change corresponds to
                    aaM = snvAaRe.match(change)
                    if (aaM):
                        aaRef, aaPos, aaAlt = aaM.groups()
                        varStartMin = (int(aaPos) - 1) * 3
                        if (genePos.get(geneName)):
                            cdsStart = genePos.get(geneName)
                            varStartMin += cdsStart
                            varStartMax = varStartMin + 2
                            for varName in branchVariants.keys():
                                ref, pos, alt = snvRe.match(varName).groups()
                                pos = int(pos)
                                if (pos >= varStartMin and pos <= varStartMax):
                                    variantAaChanges[varName] = geneName + ':' + change
                        else:
                            warn("Can't find start for gene " + geneName)
                    else:
                        warn("Can't match amino acid change" + change)
    except KeyError:
        pass
    if (branch['node_attrs'].get('clade_membership')):
        cladeName = branch['node_attrs']['clade_membership']['value']
        if (not cladeName in clades):
            clades[cladeName] = cladeFromVariants(cladeName, branchVariants)
    kids = branch.get('children')
    if (kids):
        for child in kids:
            rUnpackNextstrainTree(child, branchVariants);
    else:
        for varName in branchVariants:
            variantCounts[varName] += 1
        samples.append({ 'id': branch['node_attrs']['gisaid_epi_isl']['value'],
                         'name': branch['name'],
                         'clade': branch['node_attrs']['clade_membership']['value'],
                         'variants': branchVariants })
        if (cladeName):
            if (clades[cladeName].get('sampleCount')):
                clades[cladeName]['sampleCount'] += 1
            else:
                clades[cladeName]['sampleCount'] = 1
            if (clades[cladeName].get('samples')):
                clades[cladeName]['samples'].append(branch['name'])
            else:
                clades[cladeName]['samples'] = [ branch['name'] ]

rUnpackNextstrainTree(ncov['tree'], {})

sampleCount = len(samples)
sampleNames = [ sample['id'] + ':' + sample['name'] for sample in samples ]

# Parse variant names like 'G11083T' into pos and alleles; bundle in VCF column order
parsedVars = []
for varName in variantCounts.keys():
    m = snvRe.match(varName)
    if (m):
        ref, pos, alt = m.groups()
        parsedVars.append([int(pos), varName, ref, alt])
    else:
        warn("Can't match " + varName)
# Sort by position
def parsedVarPos(pv):
    return pv[0]
parsedVars.sort(key=parsedVarPos)

def boolToStr01(bool):
    if (bool):
        return '1'
    else:
        return '0'

with open('nextstrainSamples.vcf', 'w') as outC:
    outC.write('##fileformat=VCFv4.3\n')
    outC.write('##source=nextstrain.org\n')
    outC.write('\t'.join(['#CHROM', 'POS', 'ID', 'REF', 'ALT', 'QUAL', 'FILTER', 'INFO', 'FORMAT']) +
               '\t' + '\t'.join(sampleNames) + '\n')
    for pv in parsedVars:
        varName = pv[1]
        info = 'AC=' + str(variantCounts[varName]) + ';AN=' + str(sampleCount)
        aaChange = variantAaChanges.get(varName)
        if (aaChange):
            info += ';AACHANGE=' + aaChange
        genotypes = []
        for sample in samples:
            gt = boolToStr01(sample['variants'].get(varName))
            genotypes.append(gt + ':' + sample['clade'])
        outC.write('\t'.join([ chrom,
                               '\t'.join(map(str, pv)),
                               '\t'.join(['.', 'PASS', info, 'GT:CLADE']),
                               '\t'.join(genotypes) ]) + '\n')
    outC.close()

with open('nextstrainClade.bed', 'w') as outC:
    for name, clade in clades.items():
        if (clade.get('thickStart')):
            outC.write('\t'.join(map(str,
                                     [ chrom, 0, 29903, name, 0, '.',
                                       clade['thickStart'], clade['thickEnd'], clade['color'],
                                       len(clade['varSizes']) + 2,
                                       '1,' + ','.join(map(str, clade['varSizes'])) + ',1,',
                                       '0,' + ','.join(map(str, clade['varStarts'])) + ',29902,',
                                       ', '.join(clade['varNames']),
                                       clade['sampleCount'],
                                       ', '.join(clade['samples']) ])) + '\n')
    outC.close()
