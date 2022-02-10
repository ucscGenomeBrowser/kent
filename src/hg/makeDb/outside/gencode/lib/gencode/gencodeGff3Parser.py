"""
Rather hacked parser for the gencode GFF3.  This is not general.
"""
# tested by gencodeGff3ToAttrs
import re
from .gencodeGxfRec import Record, Attributes
from .gencodeGxfParser import GxfParser

class Gff3Parser(object):
    """There isn't a real GFF3 parser currently in BioPython. This is
    a specialized parser the assumed the GENCODE GFF3 structure."""

    def __init__(self, gff3File=None, gff3Fh=None, ignoreUnknownAttrs=False):
        self.ignoreUnknownAttrs = ignoreUnknownAttrs
        self.lineParser = GxfParser(gff3File, gff3Fh)

    def getFileName(self):
        return self.lineParser.gxfFile

    def getMetas(self):
        "must call after file has been parsed"
        return self.lineParser.metas

    def close(self):
        self.lineParser.close()

    # parses `attr=val'
    splitAttrRe = re.compile("^([a-zA-Z_]+)=(.+)$")

    def __parseAttrVal(self, attrStr):
        "returns tuple of tuple of (attr, value), multiple are returned to handle multi-value attributes"
        m = self.splitAttrRe.match(attrStr)
        if m is None:
            raise self.lineParser.parseException("can't parse attribute/value: '" + attrStr + "'")
        attr = m.group(1)
        val = m.group(2)
        if (not self.ignoreUnknownAttrs) and (attr not in Attributes.knownAttrs):
            raise self.lineParser.parseException("unknown attribute: '" + attrStr + "'")
        if attr in Attributes.multiValueAttrs:
            return self.__splitMultiValAttr(attr, val)
        else:
            return ((attr, val),)

    splitMultiVarRe = re.compile(",")

    def __splitMultiValAttr(self, attr, valStr):
        attrVals = []
        for val in self.splitMultiVarRe.split(valStr):
            attrVals.append((attr, val))
        return tuple(attrVals)

    # will not currently work if attr has quoted space or ;
    splitAttrColRe = re.compile("; *")

    def __parseAttrs(self, attrsStr):
        "parse the attributes and values"
        attrVals = []
        for attrStr in self.splitAttrColRe.split(attrsStr):
            if len(attrStr) > 0:
                attrVals.extend(self.__parseAttrVal(attrStr))
        return tuple(attrVals)

    def __parseRecord(self, line):
        gff3NumCols = 9
        row = line.split("\t")
        if len(row) != gff3NumCols:
            raise self.lineParser.parseException("Wrong number of columns, expected " + str(gff3NumCols) + ", got " + str(len(row)))
        return Record(line, self.lineParser.lineNumber, row[0], row[1], row[2], int(row[3]), int(row[4]), row[5], row[6], row[7], self.__parseAttrs(row[8]))

    def reader(self):
        "Generator over records"
        try:
            for line in self.lineParser.reader():
                yield self.__parseRecord(line)
        finally:
            self.close()
