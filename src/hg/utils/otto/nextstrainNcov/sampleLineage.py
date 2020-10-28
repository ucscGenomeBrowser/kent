#!/usr/bin/env python3

import logging, argparse, sys
import lineageColors, vcf, utils, virusNames

def main():
    parser = argparse.ArgumentParser(description="""
Read sample names from sampleFile.
Read sample IDs that are a concatenation of EPI ID, sample name and approximate date,
for resolving sampleFile IDs and lineageFile IDs, from a VCF file.
Read lineage assignments from lineageFile.
Write out 3 tab-sep columns:
sample, lineage, lineageColor.
"""
    )
    parser.add_argument('sampleFile', help='File containing sample IDs')
    parser.add_argument('vcfFile', help='VCF file with genotype columns for the sample samples')
    parser.add_argument('lineageFile', help='Two-column tab-sep file mapping sample to lineage')
    args = parser.parse_args()

    samples = utils.listFromFile(args.sampleFile)
    vcfSamples = vcf.readSamples(args.vcfFile)
    idLookup = virusNames.makeIdLookup(vcfSamples)
    lineages = utils.dictFromFile(args.lineageFile)
    nsLineages = dict([ (virusNames.maybeLookupSeqName(name, idLookup), lin)
                        for name, lin in lineages.items() ])
    for sample in samples:
        nsSample = virusNames.maybeLookupSeqName(sample, idLookup)
        lineage = nsLineages.get(nsSample)
        if (not lineage):
            lineage = ''
        color = "#%06x" % (lineageColors.lineageToColor(lineage))
        print('\t'.join([sample, lineage, color]))

main()
