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

def cladeFromVariants(name, variants, varStr):
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
        clade['varNames'] = varStr
    return clade

def numDateToMonthDay(numDate):
    """Transform decimal year timestamp to string with only month and day"""
    year = int(numDate)
    isLeapYear = 1 if (year % 4 == 0) else 0
    # Get rid of the year
    numDate -= year
    # Convert to Julian day
    jDay = int(numDate * 365) + 1
    if (jDay > 334 + isLeapYear):
        monthDay ="Dec" + str(jDay - 334 - isLeapYear)
    elif (jDay > 304 + isLeapYear):
        monthDay ="Nov" + str(jDay - 304 - isLeapYear)
    elif (jDay > 273 + isLeapYear):
        monthDay ="Oct" + str(jDay - 273 - isLeapYear)
    elif (jDay > 243 + isLeapYear):
        monthDay ="Sep" + str(jDay - 243 - isLeapYear)
    elif (jDay > 212 + isLeapYear):
        monthDay ="Aug" + str(jDay - 212 - isLeapYear)
    elif (jDay > 181 + isLeapYear):
        monthDay ="Jul" + str(jDay - 181 - isLeapYear)
    elif (jDay > 151 + isLeapYear):
        monthDay ="Jun" + str(jDay - 151 - isLeapYear)
    elif (jDay > 120 + isLeapYear):
        monthDay ="May" + str(jDay - 120 - isLeapYear)
    elif (jDay > 90 + isLeapYear):
        monthDay ="Apr" + str(jDay - 90 - isLeapYear)
    elif (jDay > 59 + isLeapYear):
        monthDay ="Mar" + str(jDay - 59 - isLeapYear)
    elif (jDay > 31):
        monthDay ="Feb" + str(jDay - 31)
    else:
        monthDay ="Jan" + str(jDay)
    return monthDay

def rUnpackNextstrainTree(branch, parentVariants, parentVarStr):
    """Recursively descend ncov.tree and build data structures for genome browser tracks"""
    # Gather variants specific to this node/branch (if any)
    localVariants = []
    if (branch.get('branch_attrs') and branch['branch_attrs'].get('mutations') and
        branch['branch_attrs']['mutations'].get('nuc')):
        # Nucleotide variants specific to this branch
        for varName in branch['branch_attrs']['mutations']['nuc']:
            if (snvRe.match(varName)):
                localVariants.append(varName)
                if (not variantCounts.get(varName)):
                    variantCounts[varName] = 0;
        # Amino acid variants: figure out which nucleotide variant goes with each
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
                            for varName in localVariants:
                                ref, pos, alt = snvRe.match(varName).groups()
                                pos = int(pos) - 1
                                if (pos >= varStartMin and pos <= varStartMax):
                                    variantAaChanges[varName] = geneName + ':' + change
                        else:
                            warn("Can't find start for gene " + geneName)
                    else:
                        warn("Can't match amino acid change" + change)
    # Inherit parent variants
    branchVariants = parentVariants.copy()
    # Add variants specific to this branch (if any)
    for varName in localVariants:
        branchVariants[varName] = 1
    # Make an ordered variant string as David requested: semicolons between nodes,
    # comma-separated within a node:
    branchVarStr = ''
    for varName in localVariants:
        if (len(branchVarStr)):
            branchVarStr += ', '
        branchVarStr += varName
        aaVar = variantAaChanges.get(varName)
        if (aaVar):
            branchVarStr += ' (' + aaVar + ')'
    if (len(parentVarStr) and len(branchVarStr)):
        branchVarStr = parentVarStr + '; ' + branchVarStr
    if (branch['node_attrs'].get('clade_membership')):
        cladeName = branch['node_attrs']['clade_membership']['value']
        if (not cladeName in clades):
            clades[cladeName] = cladeFromVariants(cladeName, branchVariants, branchVarStr)
    kids = branch.get('children')
    if (kids):
        for child in kids:
            rUnpackNextstrainTree(child, branchVariants, branchVarStr);
    else:
        for varName in branchVariants:
            variantCounts[varName] += 1
        date = numDateToMonthDay(branch['node_attrs']['num_date']['value'])
        samples.append({ 'id': branch['node_attrs']['gisaid_epi_isl']['value'],
                         'name': branch['name'],
                         'clade': branch['node_attrs']['clade_membership']['value'],
                         'date': date,
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

rUnpackNextstrainTree(ncov['tree'], {}, '')

sampleCount = len(samples)
sampleNames = [ ':'.join([sample['id'], sample['name'], sample['date']]) for sample in samples ]

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
                                       clade['varNames'],
                                       clade['sampleCount'],
                                       ', '.join(clade['samples']) ])) + '\n')
    outC.close()
