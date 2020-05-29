# Nextstrain colors encoded as RGB integers for misuse as Newick branch lengths

cladeBogusLengths = { 'A1a': 4803553, 'A2': 4948969, 'A2a': 6073807,
                      'A3': 7849892, 'A6': 10146938, 'A7': 11386193,
                      'B': 15322442, 'B1': 16756801, 'B2': 16742965,
                      'B4': 16332073 }

def addCladesAsBogusLength(node, labelToClade):
    """Associate a clade with each leaf in tree, overwriting the branch length with an RGB
    color encoded as an int.  Then on the way back up, assign clade (and length) to each
    internal node if its descendants all have the same clade."""
    if (node['kids']):
        # Internal node: do all descendants have same clade?
        kids = node['kids']
        for kid in kids:
            addCladesAsBogusLength(kid, labelToClade)
        kidClades = set([ kid.get('clade') for kid in kids ]);
        if (len(kidClades) == 1):
            node['clade'] = list(kidClades)[0]
            if (not node['label']):
                node['label'] = node['clade']
    else:
        # Leaf: look up clade by label
        node['clade'] = labelToClade.get(node['label'])
    # Update length based on clade (or lack thereof):
    clade = node.get('clade')
    if (clade and cladeBogusLengths.get(clade)):
        node['length'] = str(cladeBogusLengths[clade])
    else:
        node['length'] = str(0)

