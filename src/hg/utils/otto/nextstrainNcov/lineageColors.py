# Assign colors based on Rambaut et al. lineages.

# Commands to come up with a top 15 to highlight:
#
# git clone https://github.com/hCoV-2019/lineages.git
#
# Top 20 by sample count (including UK, 6k samples out of 15k total):
# sed -re 's/,/\t/g' lineages/lineages/data/lineages.2020-05-07.csv \
# | cut -f 6 | sort | uniq -c | sort -nr | head -20 | awk '{print $2;}' | sort > 1
#
# Top 20 excluding UK:
# sed -re 's/,/\t/g' lineages/lineages/data/lineages.2020-05-07.csv | tawk '$2 != "UK"' \
# | cut -f 6 | sort | uniq -c | sort -nr | head -20 | awk '{print $2;}' | sort > 2
#
# 15 lineages were in the top 20 either with or without UK samples:
# comm -12 1 2 > top20WithOrWithoutUk

# Repeating the analysis with the 2020-05-19 release, 16 were in both top 20's... excluded
# B.2.1 because its excluding-UK count was so low.
# http://www.perbang.dk/rgbgradient/ is handy for making color spectra.

from collections import defaultdict
import logging

# Use Nextstrain colors where there's a good match, avoid Nextstrain colors where there's not.
topLineageColors = { 'A':   0xe9cd4a, # Nextstrain B yellow
                     'A.1': 0xffb041, # Nextstrain B1 light orange
                     'A.2': 0xe2ec04, # neon yellow (orange and red are taken)
                     'A.3': 0xff66da, # pink
                     # Purple-to-green spectrum for B lineages / A clades
                     'B':       0xce66ff, # light purple
                     'B.1':     0x5cadcf, # Nextstrain A2a turquoise
                     'B.1.1':   0x975bfd,
                     'B.1.p2':  0x5a51fb,
                     'B.1.3':   0x29f3c7,
                     'B.1.5':   0x1ff181,
                     'B.1.p21': 0x16ef36,
                     'B.2':     0x494be1, # Nextstrain A1a darkish blue
                     'B.3':     0x33ed0d,
                     'B.4':     0x77c7a4, # Nextstrain A3 greenish aqua
                     'B.6':     0x74ec03
                 }

defaultColor = 0x000000

def newnode():
    """Return a new node of a tree mapping hierarchical lineages to colors"""
    return { 'kids': defaultdict(newnode), 'color': defaultColor }

def lineagesToTree(lineages):
    """Given parallel lists of lineages and colors, build a tree with the lineages in
    ancestor/descendant hierarchy and associate a color with each node."""
    tree = newnode()
    for lineage, color in topLineageColors.items():
        node = tree
        linList = lineage.split('.')
        for linLevel in linList:
            if (linLevel not in node['kids']):
                node['kids'][linLevel] = newnode()
            node = node['kids'][linLevel]
        node['color'] = color
    return tree

topLineageTree = lineagesToTree(topLineageColors)

def lineageToColor(lineage):
    """Given a lineage like 'A' or 'B.1.34', find the lineage or its nearest ancestor in
    topLineageTree and return the associated color.  Return defaultColor for no match."""
    linList = lineage.split('.')
    node = topLineageTree
    for linLevel in linList:
        if linLevel in node['kids']:
            node = node['kids'][linLevel]
        else:
            break
    return node['color']

def addLineagesAsBogusLength(node, labelToLineage, cumulativeNoLineageCount=0):
    """Associate a lineage with each leaf in tree, overwriting the branch length with an RGB
    color encoded as an int.  Then on the way back up, assign lineage (and length) to each
    internal node if its descendants all have the same lineage.  Return the number of leaves
    for which we were unable to assign a lineage."""
    noLineageCount = 0
    if (node['kids']):
        # Internal node: do all descendants have same lineage?
        kids = node['kids']
        for kid in kids:
            kidNoLinCount = addLineagesAsBogusLength(kid, labelToLineage, cumulativeNoLineageCount)
            noLineageCount += kidNoLinCount
            cumulativeNoLineageCount += kidNoLinCount
        kidLineages = set([ kid.get('lineage') for kid in kids ]);
        if (len(kidLineages) == 1):
            node['lineage'] = list(kidLineages)[0]
            if (not node['label']):
                node['label'] = node['lineage']
    else:
        # Leaf: look up lineage by label
        lineage = labelToLineage.get(node['label'])
        if (not lineage):
            if ((cumulativeNoLineageCount + noLineageCount) < 10):
                logging.warn('No lineage for "' + node['label'] + '"')
            lineage = defaultColor
            noLineageCount += 1
        node['lineage'] = lineage
    # Update length based on lineage (or lack thereof):
    lineage = node.get('lineage')
    if (lineage):
        node['length'] = str(lineageToColor(lineage))
    else:
        node['length'] = str(defaultColor)
    return noLineageCount
