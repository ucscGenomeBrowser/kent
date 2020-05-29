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

from collections import defaultdict;

spectrumColors = [0x0018E2, 0x3F00DF, 0x9500DD, 0xDA00CC, 0xD80075,
                  0xD50020, 0xD33200, 0xD08400, 0xC9CE00, 0x77CB00,
                  0x26C900, 0x00C627, 0x00C473, 0x00C1BD, 0x0078BF]

defaultColor = 0x000000

# Break top lineages into their hierarchy components so that when a lineage is not in the top
# 15, it can be assigned to its nearest ancestor, e.g. B.1.50 can be lumped in with B.1.
topLineages = [ 'A', 'A.1', 'A.2', 'B', 'B.1',
                'B.1.2', 'B.1.21', 'B.1.3', 'B.1.5', 'B.1.5.1',
                'B.2', 'B.2.1', 'B.3', 'B.4', 'B.6' ]

def newnode():
    """Return a new node of a tree mapping hierarchical lineages to colors"""
    return { 'kids': defaultdict(newnode), 'color': defaultColor }

def lineagesToTree(lineages):
    """Given parallel lists of lineages and colors, build a tree with the lineages in
    ancestor/descendant hierarchy and associate a color with each node."""
    tree = newnode()
    for lineage, color in zip(lineages, spectrumColors):
        node = tree
        linList = lineage.split('.')
        for linLevel in linList:
            if (linLevel not in node['kids']):
                node['kids'][linLevel] = newnode()
            node = node['kids'][linLevel]
        node['color'] = color
    return tree

topLineageTree = lineagesToTree(topLineages)

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

def addLineagesAsBogusLength(node, labelToLineage):
    """Associate a lineage with each leaf in tree, overwriting the branch length with an RGB
    color encoded as an int.  Then on the way back up, assign lineage (and length) to each
    internal node if its descendants all have the same lineage."""
    if (node['kids']):
        # Internal node: do all descendants have same lineage?
        kids = node['kids']
        for kid in kids:
            addLineagesAsBogusLength(kid, labelToLineage)
        kidLineages = set([ kid.get('lineage') for kid in kids ]);
        if (len(kidLineages) == 1):
            node['lineage'] = list(kidLineages)[0]
            if (not node['label']):
                node['label'] = node['lineage']
    else:
        # Leaf: look up lineage by label
        node['lineage'] = labelToLineage[node['label']]
    # Update length based on lineage (or lack thereof):
    lineage = node.get('lineage')
    if (lineage):
        node['length'] = str(lineageToColor(lineage))
    else:
        node['length'] = str(defaultColor)

