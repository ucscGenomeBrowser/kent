#!/usr/bin/env python3

import logging, argparse, sys
import lineageColors, newick, vcf, utils, virusNames

def assignColors(node, idLookup, labelToLineage):
    if (node['kids']):
        # Internal node: do all descendants have same lineage?
        kids = node['kids']
        for kid in kids:
            assignColors(kid, idLookup, labelToLineage)
        kidLineages = set([ kid.get('lineage') for kid in kids ]);
        if (len(kidLineages) == 1):
            node['lineage'] = list(kidLineages)[0]
        else:
            node['lineage'] = ''
        kidColors = set([ kid.get('color') for kid in kids ])
        if (len(kidColors) == 1):
            node['color'] = list(kidColors)[0]
            if (node['color']):
                label = node['label']
                if (not label):
                    label = str(node['inode'])
                if (label):
                    print("\t".join([ label, node['lineage'], node['color']]))
    else:
        # Leaf: look up lineage by label
        lineage = labelToLineage.get(virusNames.maybeLookupSeqName(node['label'], idLookup))
        if (not lineage):
            logging.warn('No lineage for "' + node['label'] + '"')
            lineage = ''
        node['lineage'] = lineage
        node['color'] = "#%06x" % (lineageColors.lineageToColor(lineage))
        print('\t'.join([ node['label'], lineage, node['color'] ]))

def main():
    parser = argparse.ArgumentParser(description="""
Read tree from Newick treeFile.
Read sample IDs that are a concatenation of EPI ID, sample name and approximate date,
for resolving sampleFile IDs and lineageFile IDs, from a VCF file.
Read lineage assignments from lineageFile.
Figure out what lineage and color (if any) are assigned to each leaf, and then work
back towards root assigning color to each named node whose descendants all have same color.
Write out 3 tab-sep columns:
sampleOrNode, lineage, lineageColor.
"""
    )
    parser.add_argument('treeFile', help='Newick tree whose leaf labels are sample IDs')
    parser.add_argument('vcfFile', help='VCF file with genotype columns for the sample samples')
    parser.add_argument('lineageFile', help='Two-column tab-sep file mapping sample to lineage')
    args = parser.parse_args()

    tree = newick.parseFile(args.treeFile)
    vcfSamples = vcf.readSamples(args.vcfFile)
    idLookup = virusNames.makeIdLookup(vcfSamples)
    lineages = utils.dictFromFile(args.lineageFile)
    nsLineages = dict([ (virusNames.maybeLookupSeqName(name, idLookup), lin)
                        for name, lin in lineages.items() ])
    assignColors(tree, idLookup, nsLineages)

main()
