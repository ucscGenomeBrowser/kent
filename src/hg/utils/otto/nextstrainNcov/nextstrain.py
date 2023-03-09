#!/usr/bin/env python3

import json
import re
from warnings import warn
from collections import defaultdict

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

# Variants and clades

snvRe = re.compile('^([ACGT-])([0-9]+)([ACGT-])$')
snvAaRe = re.compile('^([A-Z*-])([0-9]+)([A-Z*-])$')

newClades = {}
variantCounts = {}
variantAaChanges = {}
samples = []

# Clades from Feb. 2023:
# nColors=23
# head -$nColors ~/github/ncov/defaults/color_schemes.tsv | tail -1 \
# | perl -wne 'chomp; @cols = grep { s/^#//; } split;
#              foreach $hexC (@cols) {
#                if ($hexC =~ m/^(..)(..)(..)$/) {
#                  print sprintf("%d,%d,%d\n", hex($1), hex($2), hex($3));
#                } else { die $hexC; } }'
newCladeColors = { '20H (Beta, V2)':  '94,29,157',
                   '20I (Alpha, V1)': '74,39,178',
                   '20J (Gamma, V3)': '64,58,196',
                   '21A (Delta)' :    '63,82,205',
                   '21I (Delta)':     '65,105,207',
                   '21J (Delta)':     '69,126,203',
                   '21B (Kappa)':     '76,144,192',
                   '21C (Epsilon)':   '85,158,177',
                   '21D (Eta)':       '95,169,160',
                   '21E (Theta)':     '107,177,142',
                   '21F (Iota)':      '121,183,124',
                   '21G (Lambda)':    '137,187,107',
                   '21H (Mu)':        '153,189,93',
                   '21M (Omicron)':   '170,189,82', # B.1.1.529
                   '21K (Omicron)':   '187,188,73', # BA.1
                   '21L (Omicron)':   '203,184,66', # BA.2
                   '22A (Omicron)':   '215,175,62', # BA.4
                   '22B (Omicron)':   '224,162,58', # BA.5
                   '22C (Omicron)':   '230,146,55', # BA.2.12.1
                   '22D (Omicron)':   '230,123,50', # BA.2.75
                   '22E (Omicron)':   '227,97,45',  # BQ.1
                   '22F (Omicron)':   '223,69,40',  # XBB
                   '23A (Omicron)':   '219,40,35',  # XBB.1.5
                   # Grayscale for pre-VoC lineages
                   '19A':       '216,216,216',
                   '19B':       '209,209,209',
                   '20A':       '202,202,202',
                   '20B':       '195,195,195',
                   '20C':       '188,188,188',
                   '20D':       '181,181,181',
                   '20E (EU1)': '174,174,174',
                   '20F':       '167,167,167',
                   '20G':       '160,160,160',
         }

def cladeColorFromName(cladeName, cladeColors):
    color = cladeColors.get(cladeName);
    if (not color):
        color = '0,0,0'
    return color

def cladeFromVariants(name, variants, varStr):
    """Extract bed12 info from an object whose keys are SNV variant names"""
    clade = {}
    snvEnds = []
    # Watch out for back-mutations which make invalid BED because then we have multiple "blocks"
    # at the same position.  Instead, make a back-mutation cancel out the mutation because the
    # mutation is not found at this node.
    changesByPos = defaultdict(list)
    ixsToRemove = []
    for varName in variants:
        m = snvRe.match(varName)
        if (m):
            ref, pos, alt = m.groups()
            prevMut = changesByPos[pos]
            if (prevMut):
                # If multi-allelic, leave the position in the list; but if back-mutation,
                # remove the original mutation.  In either case, don't add this pos again.
                prevIx, prevRef, prevAlt = prevMut
                if (prevAlt == ref and prevRef == alt):
                    ixsToRemove.append(prevIx)
                    changesByPos[pos] = []
            else:
                ix = len(snvEnds)
                changesByPos[pos] = (ix, ref, alt)
                snvEnds.append(int(pos))
        else:
            warn("cladeFromVariants: no match for SNV '%s'" % (varName))
    if ixsToRemove:
        ixsToRemove.sort(reverse=True)
        for ix in ixsToRemove:
            del snvEnds[ix]
    if snvEnds:
        snvEnds.sort()
        snvStarts = [ e-1 for e in snvEnds ]
        snvSizes = [ 1 for e in snvEnds ]
        clade['thickStart'] = min(snvStarts)
        clade['thickEnd'] = max(snvEnds)
        clade['name'] = name
        clade['varSizes'] = snvSizes
        clade['varStarts'] = snvStarts
        clade['varNames'] = varStr
    return clade

def addDatesToClade(clade, numDateAttrs):
    """Add the numeric dates from ncov.json node_attrs.num_date to clade record"""
    if (numDateAttrs):
        clade['dateInferred'] = numDateAttrs['value']
        clade['dateConfMin'] = numDateAttrs['confidence'][0]
        clade['dateConfMax'] = numDateAttrs['confidence'][1]
    else:
        clade['dateInferred'] = clade['dateConfMin'] = clade['dateConfMax'] = ''

def addCountryToClade(clade, countryAttrs):
    """Add country data from ncov.json node_attrs.country to clade"""
    clade['countryInferred'] = countryAttrs['value']
    conf = countryAttrs.get('confidence')
    clade['countryConf'] = ', '.join([ "%s: %0.5f" % (country, conf)
                                       for country, conf in conf.items()]) if conf else ''

def processClade(branch, tag, branchVariants, branchVarStr, clades):
    """If this is the first time we've seen clade, add it to clades"""
    nodeAttrs = branch['node_attrs']
    if (nodeAttrs.get(tag)):
        cladeName = nodeAttrs[tag]['value']
        if (cladeName != 'unassigned' and not cladeName in clades):
            clades[cladeName] = cladeFromVariants(cladeName, branchVariants, branchVarStr)
            addDatesToClade(clades[cladeName], nodeAttrs.get('num_date'))
            if (nodeAttrs.get('country')):
                addCountryToClade(clades[cladeName], nodeAttrs['country'])
            elif (nodeAttrs.get('division')):
                addCountryToClade(clades[cladeName], nodeAttrs['division'])
            clades[cladeName]['topNode'] = branch

def numDateToYmd(numDate):
    """Convert numeric date (decimal year) to integer year, month, day"""
    year = int(numDate)
    isLeapYear = 1 if (year % 4 == 0) else 0
    # Get rid of the year
    numDate -= year
    # Convert to Julian day
    daysInYear = 366 if isLeapYear else 365
    jDay = int(numDate * daysInYear) + 1
    if (jDay > 334 + isLeapYear):
        month, day = 11, (jDay - 334 - isLeapYear)
    elif (jDay > 304 + isLeapYear):
        month, day = 10, (jDay - 304 - isLeapYear)
    elif (jDay > 273 + isLeapYear):
        month, day = 9, (jDay - 273 - isLeapYear)
    elif (jDay > 243 + isLeapYear):
        month, day = 8, (jDay - 243 - isLeapYear)
    elif (jDay > 212 + isLeapYear):
        month, day = 7, (jDay - 212 - isLeapYear)
    elif (jDay > 181 + isLeapYear):
        month, day = 6, (jDay - 181 - isLeapYear)
    elif (jDay > 151 + isLeapYear):
        month, day = 5, (jDay - 151 - isLeapYear)
    elif (jDay > 120 + isLeapYear):
        month, day = 4, (jDay - 120 - isLeapYear)
    elif (jDay > 90 + isLeapYear):
        month, day = 3, (jDay - 90 - isLeapYear)
    elif (jDay > 59 + isLeapYear):
        month, day = 2, (jDay - 59 - isLeapYear)
    elif (jDay > 31):
        month, day = 1, (jDay - 31)
    else:
        month, day = 0, jDay
    return year, month, day

months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

def numDateToMonthDay(numDate):
    """Transform decimal year timestamp to string with only month and day"""
    if not numDate:
        return '?'
    year, month, day = numDateToYmd(numDate)
    return months[month] + str(day)

def numDateToYmdStr(numDate):
    """Convert decimal year to YY-MM-DD string"""
    if numDate:
        year, month, day = numDateToYmd(numDate)
        return "%02d-%02d-%02d" % (year, month+1, day)
    else:
        return ''

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
            else:
                warn("rUnpackNextstrainTree: no snvRe match for '%s'" % (varName))
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
    elif (not len(branchVarStr)):
        branchVarStr = parentVarStr
    processClade(branch, 'clade_membership', branchVariants, branchVarStr, newClades)
    kids = branch.get('children')
    if (kids):
        for child in kids:
            rUnpackNextstrainTree(child, branchVariants, branchVarStr);
    else:
        for varName in branchVariants:
            variantCounts[varName] += 1
        nodeAttrs = branch['node_attrs']
        if (nodeAttrs.get('submitting_lab')):
            lab = nodeAttrs['submitting_lab']['value']
        else:
            lab = ''
        epiNode = nodeAttrs.get('gisaid_epi_isl')
        epiId = epiNode['value'] if epiNode else branch['name']
        numDateNode = nodeAttrs.get('num_date')
        numDate = numDateNode['value'] if numDateNode else ''
        samples.append({ 'id': epiId,
                         'name': branch['name'],
                         'clade': nodeAttrs['clade_membership']['value'],
                         'date': numDateToMonthDay(numDate),
                         'lab': lab,
                         'variants': branchVariants,
                         'varStr': branchVarStr })

rUnpackNextstrainTree(ncov['tree'], {}, '')

def sampleName(sample):
    if sample['id'] != sample['name']:
        return '|'.join([sample['id'], sample['name'], sample['date']])
    else:
        return '|'.join([sample['name'], sample['date']])

sampleCount = len(samples)
sampleNames = [ sampleName(sample)  for sample in samples ]

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

def parsedVarAlleleCount(pv):
    if not variantCounts.get(pv[1]):
        print(f'No variantCount for pv {pv}')
        exit(1)
    return variantCounts[pv[1]]

def mergeVariants(pvList):
    """Given a list of parsedVars [pos, varName, ref, alt] at the same pos, resolve the actual
    allele at each sample, handling back-mutations and serial mutations."""
    # Sort by descending allele count, assuming that the ref allele of the most frequently
    # observed variant is the true ref allele.  For back-muts, ref and alt are swapped;
    # for serial muts, alt of the first is ref of the second.
    pvList.sort(key=parsedVarAlleleCount)
    pvList.reverse()
    pos, varName, trueRef, firstAlt = pvList[0]
    alts = []
    sampleAlleles = [ 0 for sample in samples ]
    backMutSamples = []
    for pv in pvList:
        thisPos, thisName, thisRef, thisAlt = pv
        count = variantCounts[thisName]
        mergedName = trueRef + str(pos) + thisAlt
        if (thisPos != pos):
            warn("mergeVariants: inconsistent pos " + pos + " and " + thisPos)
        if (thisAlt == trueRef):
            # Back-mutation, not a true alt
            alIx = 0
        elif (thisAlt == '-'):
            # Deletion but we'll pretend it's no-call for VCF
            alIx = -1
        else:
            # Add to list of alts - unless it's an alt we've already seen, but from a different
            # serial mutation.  For example, there might be T>A but also T>G+G>A; don't add A twice.
            if (not thisAlt in alts):
                alts.append(thisAlt)
                if (thisName != varName):
                    if varName == trueRef + str(pos) + '-':
                        varName = mergedName
                    else:
                        varName += "," + mergedName
            alIx = alts.index(thisAlt) + 1
        for ix, sample in enumerate(samples):
            if (sample['variants'].get(thisName)):
                sampleAlleles[ix] = alIx
                if (alIx == 0 and thisRef != '-'):
                    backMutSamples.append(sampleName(sample))
        if thisRef != trueRef:
            # If this is a new merged mutation, add it to variantCounts.  If merging makes an
            # already-seen mutation then add to the existing count.
            if variantCounts.get(mergedName):
                variantCounts[mergedName] += count
            else:
                variantCounts[mergedName] = count
    # After handling back- and serial mutations, figure out true counts of each alt allele:
    altCounts = [ 0 for alt in alts ]
    for alIx in sampleAlleles:
        if (alIx > 0):
            altCounts[alIx - 1] += 1
    return [ [pos, varName, trueRef, ','.join(alts)],
             alts, altCounts, sampleAlleles, backMutSamples ]

mergedVars = []

variantsAtPos = []
for pv in parsedVars:
    pos = pv[0]
    if (len(variantsAtPos) == 0 or pos == variantsAtPos[0][0]):
        variantsAtPos.append(pv)
    else:
        mv = mergeVariants(variantsAtPos)
        if (mv[1]):
            mergedVars.append(mv)
        variantsAtPos = [pv]
mv = mergeVariants(variantsAtPos)
if (mv[1]):
    mergedVars.append(mv)

def writeVcfHeaderExceptSamples(outF):
    """Write VCF header lines -- except for sample names (this ends with a \t not a \n)."""
    outF.write('##fileformat=VCFv4.3\n')
    outF.write('##source=nextstrain.org\n')
    outF.write('\t'.join(['#CHROM', 'POS', 'ID', 'REF', 'ALT', 'QUAL', 'FILTER', 'INFO', 'FORMAT']) +
               '\t');

def tallyAaChanges(varNameMerged):
    """If any of the merged variants cause a coding change, then produce a comma-sep string in same order as variants with corresponding change(s) or '-' if a variant does not cause a coding change.  If none of the variants cause a coding change, return the empty string."""
    varNames = varNameMerged.split(',')
    gotAaChange = 0
    aaChanges = []
    for varName in varNames:
        aaChange = variantAaChanges.get(varName)
        if (aaChange):
            gotAaChange = 1
            aaChanges.append(aaChange)
        else:
            aaChanges.append('-');
    if (gotAaChange):
        aaChangeStr = ','.join(aaChanges)
    else:
        aaChangeStr = ''
    return aaChangeStr

# VCF
def vcfForAll(fileName, cladeTag):
    with open(fileName, 'w') as outC:
        writeVcfHeaderExceptSamples(outC)
        outC.write('\t'.join(sampleNames) + '\n')
        for mv in mergedVars:
            pv, alts, altCounts, sampleAlleles, backMutSamples = mv
            info = 'AC=' + ','.join(map(str, altCounts)) + ';AN=' + str(sampleCount)
            varNameMerged = pv[1]
            aaChange = tallyAaChanges(varNameMerged)
            if (len(aaChange)):
                info += ';AACHANGE=' + aaChange
            if (len(backMutSamples)):
                info += ';BACKMUTS=' + ','.join(backMutSamples)
            genotypes = []
            for sample, alIx in zip(samples, sampleAlleles):
                gt = '.' if alIx < 0 else str(alIx)
                genotypes.append(gt + ':' + sample[cladeTag])
            outC.write('\t'.join([ chrom,
                                   '\t'.join(map(str, pv)),
                                   '\t'.join(['.', 'PASS', info, 'GT:CLADE']),
                                   '\t'.join(genotypes) ]) + '\n')

vcfForAll('nextstrainSamples.vcf', 'clade')
# I'll skip writing an enormous file with wasteful clades-in-genotypes... really need
# a sample metadata file!

# Assign samples to clades; a sample can appear in multiple clades if they are nested.

def sampleIdsFromNode(node, cladeTops=()):
    """Return a list of IDs of all samples found under node."""
    kids = node.get('children')
    if (kids):
        sampleIds = []
        for kid in kids:
            if (kid not in cladeTops):
                sampleIds += sampleIdsFromNode(kid, cladeTops)
    else:
        epiNode = node['node_attrs'].get('gisaid_epi_isl')
        sampleId = epiNode['value'] if epiNode else node['name']
        sampleIds = [sampleId]
    return sampleIds

def sanitizeFileName(filename):
    """Remove or replace characters that cause trouble in filenames"""
    filename = filename.replace('/', '_').replace(' ', '_')
    filename = filename.replace('(', '').replace(')', '')
    filename = filename.replace(',', '');
    return filename

cladeSampleCounts = {}
cladeSampleNames = {}

def vcfForClades(clades, cladeTops=()):
    """Given a set of clades, dump out VCF for each clade.
    Stop at nodes in cladeTops (for Nextstrain's new clade scheme where 19A is root, 20A fully
    contains 20B and 20C, etc."""
    for cladeName in clades:
        node = clades[cladeName]['topNode']
        cladeSampleIds = set(sampleIdsFromNode(node, cladeTops))
        cladeSampleList = [ sample for sample in samples if sample['id'] in cladeSampleIds ]
        cladeSampleCounts[cladeName] = len(cladeSampleList)
        cladeSampleNames[cladeName] = [ sampleName(sample) for sample in cladeSampleList ]
        sanitizedCladeName = sanitizeFileName(cladeName)
        with open('nextstrainSamples' + sanitizedCladeName + '.vcf', 'w') as outV:
            writeVcfHeaderExceptSamples(outV)
            outV.write('\t'.join(cladeSampleNames[cladeName]) + '\n')
            for mv in mergedVars:
                pv, alts, overallAltCounts, sampleAlleles, backMutSamples = mv
                varNameMerged = pv[1]
                genotypes = []
                altCounts = [ 0 for alt in alts ]
                acTotal=0
                for sample, alIx in zip(samples, sampleAlleles):
                    if (sample['id'] in cladeSampleIds):
                        gt = '.' if (alIx < 0) else str(alIx)
                        genotypes.append(gt + ':' + cladeName)
                        if (alIx > 0):
                            altCounts[alIx - 1] += 1
                            acTotal += 1
                if (acTotal > 0):
                    info = 'AC=' + ','.join(map(str, altCounts))
                    info += ';AN=' + str(cladeSampleCounts[cladeName])
                    aaChange = tallyAaChanges(varNameMerged)
                    if (len(aaChange)):
                        info += ';AACHANGE=' + aaChange
                    cladeBackMuts = [ sampleName for sampleName in backMutSamples
                                      if sampleName in cladeSampleNames[cladeName] ]
                    if (len(cladeBackMuts)):
                        info += ';BACKMUTS=' + ','.join(cladeBackMuts)
                    outV.write('\t'.join([ chrom,
                                           '\t'.join(map(str, pv)),
                                           '\t'.join(['.', 'PASS', info, 'GT:CLADE']),
                                           '\t'.join(genotypes) ]) + '\n')

def bedForClades(fileName, clades, cladeColors):
    """Make a BED file summarizing each clade"""
    with open(fileName, 'w') as outC:
        for name, clade in clades.items():
            if (not clade.get('thickStart')):
                # "Clade" 19A encompasses the entire tree (minus the parts assigned to
                # other "clades").  It has no identifying variants, and (as of June 7)
                # no dates assigned.
                clade['thickStart'] = clade['thickEnd'] = 0
                clade['varStarts'] = clade['varSizes'] = []
                clade['varNames'] = ''
                clade['dateInferred'] = clade['dateConfMin'] = clade['dateConfMax'] = 0
            countryConf = clade.get('countryConf')
            if (not countryConf):
                countryConf = ''
            countryInferred = clade.get('countryInferred')
            if (not countryInferred):
                countryInferred = ''
            outC.write('\t'.join(map(str,
                                     [ chrom, 0, 29903, name, 0, '.',
                                       clade['thickStart'], clade['thickEnd'],
                                       cladeColorFromName(name, cladeColors),
                                       len(clade['varSizes']) + 2,
                                       ','.join(map(str, ([1] + clade['varSizes']) + [1])),
                                       ','.join(map(str, ([0] + clade['varStarts']) + [29902])),
                                       clade['varNames'],
                                       numDateToYmdStr(clade['dateInferred']),
                                       numDateToYmdStr(clade['dateConfMin']),
                                       numDateToYmdStr(clade['dateConfMax']),
                                       countryInferred,
                                       countryConf,
                                       cladeSampleCounts[name],
                                       ', '.join(cladeSampleNames[name]) ])) + '\n')

newCladeTops = [ newClades[cladeName]['topNode'] for cladeName in newClades ]
vcfForClades(newClades, newCladeTops)
bedForClades('nextstrainClade.bed', newClades, newCladeColors)

# Newick-formatted tree of samples for VCF display
def cladeRgbFromName(cladeName, cladeColors):
    """Look up the r,g,b string color for clade; convert to int RGB."""
    rgbCommaStr = cladeColorFromName(cladeName, cladeColors)
    r, g, b = [ int(x) for x in rgbCommaStr.split(',') ]
    rgb = (r << 16) | (g << 8) | b
    return rgb

def rNextstrainToNewick(node, cladeColors, cladeTops=(), parentClade=None, parentVarStr=''):
    """Recursively descend ncov.tree and build Newick tree string of samples to file.
    Exclude nodes in cladeTops."""
    kids = node.get('children')
    if (kids):
        # Make a more concise variant path string than the one we make for the clade track,
        # to embed in internal node labels for Yatish's tree explorations.
        localVariants = []
        if (node.get('branch_attrs') and node['branch_attrs'].get('mutations') and
            node['branch_attrs']['mutations'].get('nuc')):
            # Nucleotide variants specific to this branch
            for varName in node['branch_attrs']['mutations']['nuc']:
                if (snvRe.match(varName)):
                    localVariants.append(varName)
        varStr = '+'.join(localVariants)
        if (len(parentVarStr) and len(varStr)):
            varStr = '$'.join([parentVarStr, varStr])
        elif (not len(varStr)):
            varStr = parentVarStr
        nodeAttrs = node['node_attrs']
        if (nodeAttrs.get('clade_membership')):
            cladeName = nodeAttrs['clade_membership']['value']
        elif (parentClade):
            cladeName = parentClade
        else:
            cladeName = 'unassigned'
        color = str(cladeRgbFromName(cladeName, cladeColors))
        cladeShortened = cladeName.split(' ', 1)[0]
        cladeShortened = cladeShortened.split('/', 1)[0]
        descendants = ','.join([ rNextstrainToNewick(child, cladeColors, cladeTops, cladeName,
                                                     varStr)
                                 for child in kids if child not in cladeTops ])
        treeString = '(' + descendants + '):' + color
    else:
        nodeAttrs = node['node_attrs']
        epiNode = nodeAttrs.get('gisaid_epi_isl')
        gId = epiNode['value'] if epiNode else node['name']
        name = node['name']
        numDateNode = nodeAttrs.get('num_date')
        date = numDateToMonthDay(numDateNode['value'] if numDateNode else '')
        cladeName = nodeAttrs['clade_membership']['value']
        color = str(cladeRgbFromName(cladeName, cladeColors))
        treeString = sampleName({ 'id': gId, 'name': name, 'date': date }) + ':' + color
    return treeString

with open('nextstrain.nh', 'w') as outF:
    outF.write(rNextstrainToNewick(ncov['tree'], newCladeColors) + ';\n')


def newickForClades(clades, cladeColors, cladeTops=()):
    for cladeName in clades:
        filename = 'nextstrain' + sanitizeFileName(cladeName) + '.nh'
        node = clades[cladeName]['topNode']
        with open(filename, 'w') as outF:
            outF.write(rNextstrainToNewick(node, cladeColors, cladeTops) + ';\n')

newickForClades(newClades, newCladeColors, newCladeTops)

# File with samples and their clades, labs and variant paths

apostropheSRe = re.compile("'s");
firstLetterRe = re.compile('(\w)\w+');
spacePunctRe = re.compile('\W');

def abbreviateLab(lab):
    """Lab names are very long and sometimes differ by punctuation or typos.  Abbreviate for easier comparison."""
    labAbbrev = apostropheSRe.sub('', lab)
    labAbbrev = firstLetterRe.sub(r'\1', labAbbrev, count=0)
    labAbbrev = spacePunctRe.sub('', labAbbrev, count=0)
    return labAbbrev

with open('nextstrainSamples.varPaths', 'w') as outF:
    errCount = 0
    for sample in samples:
        lab = sample['lab']
        labAbbrev = abbreviateLab(lab)
        try:
            outF.write('\t'.join([sampleName(sample), sample['clade'], labAbbrev, lab,
                                  sample['varStr']]) + '\n');
        except:
            if (errCount == 0):
                print("Problem writing varPaths for sample '", sampleName(sample), "', varStr '",
                      sample['varStr'], "'")
            errCount += 1

# Narrow down variants to "informative" set (bi-allelic, each allele supported by
# sufficient number of samples):
minSamples = 2
discardedAlleles = []
blacklist = []
informativeVariants = []

for mv in mergedVars:
    pv, alts, altCounts, sampleAlleles, backMutSamples = mv
    pos, varNameMerged, ref, altStr = pv
    recurrentAlts = []
    recurrentAltCounts = []
    for alt, altCount in zip(alts, altCounts):
        if (altCount < minSamples):
            discardedAlleles.append([ chrom, pos-1, pos, ref + str(pos) + alt ])
        else:
            recurrentAlts.append(alt)
            recurrentAltCounts.append(altCount)
    if (len(recurrentAlts) > 1):
        multiRecurrentName = ','.join([ ref + str(pos) + alt for alt in recurrentAlts ])
        blacklist.append([ chrom, pos-1, pos, multiRecurrentName ])
    elif (len(recurrentAlts) == 1):
        informativeVariants.append([ pos, ref, recurrentAlts[0] ])

# Dump out BED files for the categories:
with open('nextstrainDiscarded.bed', 'w') as outF:
    for da in discardedAlleles:
        outF.write('\t'.join(map(str, da)) + '\n');

with open('nextstrainBlacklisted.bed', 'w') as outF:
    for bl in blacklist:
        outF.write('\t'.join(map(str, bl)) + '\n');

with open('nextstrainInformative.bed', 'w') as outF:
    for iv in informativeVariants:
        pos, ref, alt = iv
        outF.write('\t'.join(map(str, [ chrom, pos-1, pos, ref + str(pos) + alt ])) + '\n')

# Compute parsimony score for tree at each informative variant.

anyDiscordantVariants = []

def refAltCosts(node, pos, ref, alt, parentValue):
    nodeValue = parentValue
    mutCount = 0
    # If there are mutations on this branch, and one of them is at pos and is ref or alt
    # (i.e. not a discarded allele that we're ignoring), then change nodeValue.
    if (node.get('branch_attrs') and node['branch_attrs'].get('mutations') and
        node['branch_attrs']['mutations'].get('nuc')):
        for varName in node['branch_attrs']['mutations']['nuc']:
            m = snvRe.match(varName)
            if (m):
                mRef, mPos, mAlt = m.groups()
                mPos = int(mPos)
                if (mPos == pos and (mAlt == ref or mAlt == alt)):
                    nodeValue = mAlt
                    mutCount += 1
            else:
                warn("refAltCosts: no snvRe match for '%s'" % (varName))
    kids = node.get('children')
    if (kids):
        refCost, altCost = [0, 0]
        for kid in kids:
            kidRefCost, kidAltCost, kidMutCount = refAltCosts(kid, pos, ref, alt, nodeValue)
            refCost += min(kidRefCost, kidAltCost+1)
            altCost += min(kidRefCost+1, kidAltCost)
            mutCount += kidMutCount
    else:
        refCost = 0 if (nodeValue == ref) else 1
        altCost = 1 if (nodeValue == ref) else 0
    if ((refCost < altCost and nodeValue == alt) or
        (altCost < refCost and nodeValue == ref)):
        anyDiscordantVariants.append('\t'.join([ref + str(pos) + alt, node['name']]))
    return refCost, altCost, mutCount

parsimonyScores = []
mutationCounts = []
rootDiscordantVariants = []

for iv in informativeVariants:
    pos, ref, alt = iv
    varName = ref + str(pos) + alt
    rootRefCost, rootAltCost, mutCount = refAltCosts(ncov['tree'], pos, ref, alt, ref)
    parsimonyScores.append([pos, min(rootRefCost, rootAltCost)])
    mutationCounts.append([pos, mutCount])
    rootLabel = 0 if (rootRefCost <= rootAltCost) else 1
    if (rootLabel != 0):
        # Note: so far this has not happened.  Seems like it would be pretty lame if it did.
        rootDiscordantVariants.append([chrom, pos-1, pos, varName])

# Write out files with discordant nodes, parsimony/mutation counts (identical!):
with open('nextstrainRootDiscordant.bed', 'w') as outF:
    for dv in rootDiscordantVariants:
        outF.write('\t'.join(dv) + '\n');

with open('nextstrainAnyDiscordant.txt', 'w') as outF:
    for varName in anyDiscordantVariants:
        outF.write(varName + '\n');

# bedGraph for parsimony scores and mutation counts
with open('nextstrainParsimony.bedGraph', 'w') as outF:
    for ps in parsimonyScores:
        pos, score = ps
        outF.write('\t'.join(map(str, [ chrom, pos-1, pos, score ])) + '\n')

with open('nextstrainMutCounts.bedGraph', 'w') as outF:
    for ps in mutationCounts:
        pos, score = ps
        outF.write('\t'.join(map(str, [ chrom, pos-1, pos, score ])) + '\n')

# Informative-only VCF
with open('nextstrainRecurrentBiallelic.vcf', 'w') as outF:
    writeVcfHeaderExceptSamples(outF)
    outF.write('\t'.join(sampleNames) + '\n')
    for iv in informativeVariants:
        pos, ref, alt = iv
        varName = ref + str(pos) + alt
        # Ignore any discarded variants at this position, but handle back-mutations if any,
        # by calling mergeVariants on this and its back-mutation
        #*** This doesn't work when iv has already been merged -- samples have the original pv,
        #*** not the merged iv, so altCounts end up being 0 for those.  But nobody uses this anyway,
        #*** should probably get rid of it...
        backMutVarName = alt + str(pos) + ref
        variants = [ [ pos, varName, ref, alt ] ]
        if (variantCounts.get(backMutVarName)):
            variants.append([ pos, backMutVarName, alt, ref ])
        pv, alts, altCounts, sampleAlleles, backMutSamples = mergeVariants(variants)
        if (len(alts) != 1):
            warn('Expected exactly one alt from merging ' + varName + ' and ' + backMutVarName +
                 ', but got [' + ', '.join(alts) + ']')
        info = 'AC=' + str(altCounts[0])
        info += ';AN=' + str(sampleCount)
        aaChange = tallyAaChanges(varName)
        if (len(aaChange)):
            info += ';AACHANGE=' + aaChange
        if (len(backMutSamples)):
            info += ';BACKMUTS=' + ','.join(backMutSamples)
        genotypes = []
        for sample, alIx in zip(samples, sampleAlleles):
            gt = '.' if (alIx < 0) else str(alIx)
            genotypes.append(gt + ':' + sample['clade'])
        outF.write('\t'.join([ '\t'.join([ chrom, str(pos), varName, ref, alt,
                                           '.', 'PASS', info, 'GT:CLADE']),
                               '\t'.join(genotypes) ]) + '\n')
