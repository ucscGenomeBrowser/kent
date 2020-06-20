#!/usr/bin/env python3

import logging, argparse, sys
import nextstrainVcf, utils, virusNames

def main():
    parser = argparse.ArgumentParser(description="""
Read samples and clade assignments from a Nextstrain VCF file.
Read lineage assignments from lineageFile.  Write out 3 tab-sep columns:
NS sample ID, clade, lineage.
"""
    )
    parser.add_argument('vcfFile', help='VCF file derived from Nextstrain data')
    parser.add_argument('lineageFile', help='Two-column tab-sep file mapping sample to lineage')
    args = parser.parse_args()

    (vcfSamples, vcfSampleClades) = nextstrainVcf.readVcfSampleClades(args.vcfFile)
    idLookup = virusNames.makeIdLookup(vcfSamples)
    lineages = utils.dictFromFile(args.lineageFile)
    nsLineages = dict([ (virusNames.maybeLookupSeqName(name, idLookup), lin)
                        for name, lin in lineages.items() ])
    for sample, clade in vcfSampleClades.items():
        lineage = nsLineages.get(sample)
        if (not lineage):
            lineage = ''
        print('\t'.join([sample, clade, lineage]))

main()
