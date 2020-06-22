
import gzip
import utils

def countAlleles(columns):
    """For each alt allele in the 5th column, keep a count of how many times that alt's index
    appears in the genotype columns, for the AC tag in the INFO field.  For the AN tag, tally up
    the genotype columns that don't have '.' (no call).  Return the total number of calls for AN
    and an array of alternate allele counts for AC.
    NOTE: this does not handle multiploid VCF where genotypes can be /- or |-separated."""
    totalCalls = 0
    alts = columns[4]
    altCounts = [ 0 for alt in alts.split(',')]
    for colVal in columns[9:]:
        subVals = colVal.split(':')
        gt = subVals[0]
        if (gt is not '.'):
            totalCalls += 1
            altIx = int(gt)-1
            if (altIx >= 0):
                altCounts[altIx] += 1
    return totalCalls, altCounts

def infoReplaceAcAn(oldInfo, totalCalls, altCounts):
    """If oldInfo includes AC and AN, strip them out.  Add AN (totalCalls) and AC (altCounts)."""
    infoParts = oldInfo.split(';')
    newInfoParts = []
    for tagVal in infoParts:
        (tag, val) = tagVal.split('=')
        if (tag not in ('AC', 'AN')):
            newInfoParts.append(tagVal)
    newInfoParts.append('AC=' + ','.join(map(str, altCounts)))
    newInfoParts.append('AN=%d' % (totalCalls))
    return ';'.join(newInfoParts)

def pruneToSamples(vcfIn, sampleSet, vcfOut):
    """Copy data in vcfIn to vcfOut, but keeping only the samples in sampleSet.
    Update AC and AN with new allele counts."""
    with gzip.open(vcfIn, 'rt') if vcfIn.endswith('.gz') else open(vcfIn, 'r') as inF:
        with open(vcfOut, 'w') as outF:
            colOutCount = 0
            for line in inF:
                if (line.startswith('#CHROM')):
                    colNamesIn = line.split('\t')
                    colNamesIn[-1] = colNamesIn[-1].rstrip()
                    colNamesOut = colNamesIn[0:9]
                    columnsKept = [True for i in range(0, 9)]
                    for vcfSample in colNamesIn[9:]:
                        if (vcfSample in sampleSet):
                            colNamesOut.append(vcfSample)
                            columnsKept.append(True)
                        else:
                            columnsKept.append(False)
                    colOutCount = len(colNamesOut)
                    outF.write('\t'.join(colNamesOut) + '\n');
                elif (line[0] == '#'):
                    outF.write(line);
                else:
                    columnsIn = line.split('\t')
                    columnsIn[-1] = columnsIn[-1].rstrip()
                    columnsOut = []
                    for ix, colVal in enumerate(columnsIn):
                        if (columnsKept[ix]):
                            columnsOut.append(colVal)
                    # Update allele counts from new set of genotypes.
                    (totalCalls, altCounts) = countAlleles(columnsOut)
                    # If this set of genotypes has none of the alt alleles, skip it.
                    if (sum(altCounts) > 0):
                        columnsOut[7] = infoReplaceAcAn(columnsIn[7], totalCalls, altCounts)
                        outF.write('\t'.join(columnsOut) + '\n')

def readSamples(vcfFile):
    """Read VCF sample IDs from the #CHROM line"""
    samples = []
    with gzip.open(vcfFile, 'rt') if vcfFile.endswith('.gz') else open(vcfFile, 'r') as vcfF:
        for line in vcfF:
            if (line.startswith('#CHROM')):
                samples = line.split('\t')[9:]
                samples[-1] = samples[-1].rstrip()
                break
            else:
                if (len(line) > 100):
                   utils.die("Line too long (%d) and no #chrom yet" % (len(line)))
    return samples

