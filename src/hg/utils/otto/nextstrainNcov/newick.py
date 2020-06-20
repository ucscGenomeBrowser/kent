# Parse Newick-formatted file into tree of { 'kids': [], 'label': '', 'length': '' }

import logging
from utils import die

def skipSpaces(treeString, offset):
    while (offset != len(treeString) and treeString[offset].isspace()):
        offset += 1
    return offset

def parseQuoted(treeString, offset):
    """Read in a quoted, possibly \-escaped string in treeString at offset"""
    label = ''
    quoteChar = treeString[offset]
    offset += 1
    labelStart = offset
    while (offset != len(treeString) and treeString[offset] != quoteChar):
        if (treeString[offset] == '\\'):
            offset += 1
        offset += 1
    if (treeString[offset] != quoteChar):
        die("Missing end-" + quoteChar + " after '" + treeString + "'")
    else:
        label = treeString[labelStart:offset]
        offset = skipSpaces(treeString, offset + 1)
    return (label, offset)

def terminatesLabel(treeString, offset):
    """Return True if treeString+offset is empty or starts w/char that would terminate a label"""
    return (offset == len(treeString) or treeString[offset] == ',' or treeString[offset] == ')' or
            treeString[offset] == ';' or treeString[offset] == ':')

def parseLabel(treeString, offset):
    """Read in a possibly quoted, possibly \-escaped node label terminated by [,):;]"""
    if (offset == len(treeString)):
        label = ''
    elif (treeString[offset] == "'" or treeString[offset] == '"'):
        (label, offset) = parseQuoted(treeString, offset)
    else:
        labelStart = offset
        while (not terminatesLabel(treeString, offset)):
            if (treeString[offset] == '\\'):
                offset += 1
            offset += 1
        label = treeString[labelStart:offset]
        offset = skipSpaces(treeString, offset)
    return(label, offset)

def parseLength(treeString, offset):
    """If treeString[offset] is ':', then parse the number that follows; otherwise return 0.0."""
    if (offset != len(treeString) and treeString[offset] == ':'):
        # Branch length
        offset = skipSpaces(treeString, offset + 1)
        if (not treeString[offset].isdigit()):
            die("Expected number to follow ':' but instead got '" +
                treeString[offset:offset+100] + "'")
        lengthStart = offset
        while (offset != len(treeString) and
               (treeString[offset].isdigit() or treeString[offset] == '.' or
                treeString[offset] == 'E' or treeString[offset] == 'e' or
                treeString[offset] == '-')):
            offset += 1
        lengthStr = treeString[lengthStart:offset]
        offset = skipSpaces(treeString, offset)
        return (lengthStr, offset)
    else:
        return ('', offset)

def parseBranch(treeString, offset, internalNode):
    """Recursively parse Newick branch (x, y, z)[label][:length] from treeString at offset"""
    if (treeString[offset] != '('):
        die("parseBranch called on treeString that doesn't begin with '(': '" +
            treeString + "'")
    branchStart = offset
    internalNode += 1
    branch = { 'kids': [],  'label': '', 'length': '', 'inode': internalNode }
    offset = skipSpaces(treeString, offset + 1)
    while (offset != len(treeString) and treeString[offset] != ')' and treeString[offset] != ';'):
        (child, offset, internalNode) = parseString(treeString, offset, internalNode)
        branch['kids'].append(child)
        if (treeString[offset] == ','):
            offset = skipSpaces(treeString, offset + 1)
    if (offset == len(treeString)):
        die("Input ended before ')' for '" + treeString[branchStart:branchStart+100] + "'")
    if (treeString[offset] == ')'):
        offset = skipSpaces(treeString, offset + 1)
    else:
        die("Can't find ')' matching '" + treeString[branchStart:branchStart+100] + "', " +
            "instead got '" + treeString[offset:offset+100] + "'")
    (branch['label'], offset) = parseLabel(treeString, offset)
    (branch['length'], offset) = parseLength(treeString, offset)
    return (branch, offset, internalNode)

def parseString(treeString, offset=0, internalNode=0):
    """Recursively parse Newick tree from treeString"""
    offset = skipSpaces(treeString, offset)
    if (treeString[offset] == '('):
        return parseBranch(treeString, offset, internalNode)
    else:
        (label, offset) = parseLabel(treeString, offset)
        (length, offset) = parseLength(treeString, offset)
        leaf = { 'kids': None, 'label': label, 'length': length }
        return (leaf, offset, internalNode)

def parseFile(treeFile):
    """Read Newick file, return tree object"""
    with open(treeFile, 'r') as treeF:
        line1 = treeF.readline().strip()
        if (line1 == ''):
            return None
        (tree, offset, internalNode) = parseString(line1)
        if (offset != len(line1) and line1[offset] != ';'):
            die("Tree terminated without ';' before '" + line1[offset:offset+100] + "'")
        treeF.close()
    return tree

def treeToString(node, pretty=False, indent=0):
    """Return a Newick string encoding node and its descendants, optionally pretty-printing with
    newlines and indentation.  String is not ';'-terminated, caller must do that."""
    if not node:
        return ''
    labelLen = ''
    if (node['label']):
        labelLen += node['label']
    if (node['length']):
        labelLen += ':' + node['length']
    if (node['kids']):
        string = '('
        kidIndent = indent + 1
        kidStrings = [ treeToString(kid, pretty, kidIndent) for kid in node['kids'] ]
        sep = ','
        if (pretty):
            sep = ',\n' + ' ' * kidIndent
        string += sep.join(kidStrings)
        string += ')'
        string += labelLen
    else:
        string = labelLen
    return string;

def printTree(tree, pretty=False, indent=0):
    """Print out Newick text encoding tree, optionally pretty-printing with
    newlines and indentation."""
    print(treeToString(tree, pretty, indent) + ';')

def leafNames(node):
    """Return a list of labels of all leaf descendants of node"""
    if (node['kids']):
        return [ leaf for kid in node['kids'] for leaf in leafNames(kid) ]
    else:
        return [ node['label'] ]

def treeIntersectIds(node, idLookup, sampleSet, lookupFunc=None):
    """For each leaf in node, attempt to look up its label in idLookup; replace if found.
    Prune nodes with no matching leaves.  Store new leaf labels in sampleSet.
    If lookupFunc is given, it is passed two arguments (label, idLookup) and returns a
    possible empty list of matches."""
    if (node['kids']):
        # Internal node: prune
        prunedKids = []
        for kid in (node['kids']):
            kidIntersected = treeIntersectIds(kid, idLookup, sampleSet, lookupFunc)
            if (kidIntersected):
                prunedKids.append(kidIntersected)
        if (len(prunedKids) > 1):
            node['kids'] = prunedKids
        elif (len(prunedKids) == 1):
            node = prunedKids[0]
        else:
            node = None
    else:
        # Leaf: lookup, prune if not found
        label = node['label']
        if (lookupFunc):
            matchList = lookupFunc(node['label'], idLookup)
        elif label in idLookup:
            matchList = idLookup[label]
        else:
            matchList = []
        if (not matchList):
            logging.info("No match for leaf '" + label + "'")
            node = None
        else:
            if (len(matchList) != 1):
                logging.warn("Non-unique match for leaf '" + label + "': ['" +
                             "', '".join(matchList) + "']")
            else:
                logging.debug(label + ' --> ' + matchList[0]);
            node['label'] = matchList[0]
            sampleSet.add(matchList[0])
    return node

