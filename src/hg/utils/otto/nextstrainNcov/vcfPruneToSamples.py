#!/usr/bin/env python3

import logging, argparse, sys
import random
import utils, vcf, virusNames

def main():
    parser = argparse.ArgumentParser(description="""
Read in VCF, read in samples from sampleFile, attempt to match VCF IDs with samples,
remove VCF genotype columns for samples not found in sampleFile, output VCF with
updated AC and AN counts.
"""
    )
    parser.add_argument('vcfFile', help='VCF file with sample genotype columns')
    parser.add_argument('sampleFile', help='File with one sample ID per line')
    parser.add_argument('vcfOutFile', help='VCF output file with only samples in sampleFile')
    args = parser.parse_args()
    samples = utils.listFromFile(args.sampleFile)
    idLookup = virusNames.makeIdLookup(samples)
    for key in idLookup:
        values = idLookup[key]
        if (len(values) != 1):
            logging.warn('Duplicate name/component in ' + args.sampleFile + ': ' + key + " -> " +
                         ", ".join(values))
    vcfSamples = vcf.readSamples(args.vcfFile)
    foundSampleSet = set([ sample for sample in vcfSamples
                           if virusNames.lookupSeqName(sample, idLookup) ])
    vcf.pruneToSamples(args.vcfFile, foundSampleSet, args.vcfOutFile)
    if (len(foundSampleSet) < len(samples)):
        logging.warn("%s has %d samples but pruned VCF has %d samples (%d samples not found)" %
                     (args.sampleFile, len(samples), len(foundSampleSet),
                      len(samples) - len(foundSampleSet)))
        allSampleSet = set([ virusNames.maybeLookupSeqName(sample, idLookup) for sample in samples ])
        sampleFileNotVcf = allSampleSet - foundSampleSet
        logging.warn("Example samples not found:\n" +
                     "\n".join(random.sample(sampleFileNotVcf, 10)))

main()
