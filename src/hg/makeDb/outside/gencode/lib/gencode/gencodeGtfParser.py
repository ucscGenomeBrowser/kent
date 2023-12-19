"""
Rather hacked parser for the gencode GTF.  This is not general.
"""
# tested by gencodeGtfToAttrs
import re
from .gencodeGxfRec import Record, Attributes
from .gencodeGxfParser import GxfParser

class GtfParser(object):
    """There isn't a real GTF parser currently in BioPython. This is
    a specialized parser the assumed the GENCODE GTF structure."""

    def __init__(self, gtfFile=None, gtfFh=None, ignoreUnknownAttrs=False):
        self.ignoreUnknownAttrs = ignoreUnknownAttrs
        self.lineParser = GxfParser(gtfFile, gtfFh)

    def getFileName(self):
        return self.lineParser.gxfFile

    def getMetas(self):
        "must call after file has been parsed"
        return self.lineParser.metas

    def close(self):
        self.lineParser.close()

    parHackOldIdRe = re.compile("^(ENS.)R([0-9.]+)$")

    def __transformParHackId(self, ensId):
        """transform ids like ENSGR0000182484 or ENST00000464205_PAR_Y to
        ENST00000464205, which were hacked for PAR"""
        m = self.parHackOldIdRe.search(ensId)
        if m is not None:
            ensId = m.group(1) + "0" + m.group(2)
        ensId = ensId.replace("_PAR_Y", "")
        return ensId

    # parses `attr "strval"' or `attr numval'
    splitAttrRe = re.compile("^([a-zA-Z_]+) +((\"(.+)\")|([0-9]+))$")

    def __parseAttrVal(self, attrStr):
        "returns tuple of (attr, value)"
        m = self.splitAttrRe.match(attrStr)
        if m is None:
            raise self.lineParser.parseException("can't parse attribute/value: '" + attrStr + "'")
        attr = m.group(1)
        val = m.group(5) if m.group(5) is not None else m.group(4)
        if (not self.ignoreUnknownAttrs) and (attr not in Attributes.knownAttrs):
            raise self.lineParser.parseException("unknown attribute: '" + attrStr + "'")
        if attr in ("gene_id", "transcript_id"):
            val = self.__transformParHackId(val)
        return (attr, val)

    # will not currently work if attr has quoted space or ;
    splitAttrColRe = re.compile("; *")

    def __parseAttrs(self, attrsStr):
        "parse the attributes and values"
        attrVals = []
        for attrStr in self.splitAttrColRe.split(attrsStr):
            if len(attrStr) > 0:
                attrVals.append(self.__parseAttrVal(attrStr))
        return tuple(attrVals)

    def __parseRecord(self, line):
        gtfNumCols = 9
        row = line.split("\t")
        if len(row) != gtfNumCols:
            raise self.lineParser.parseException("Wrong number of columns, expected " + str(gtfNumCols) + ", got " + str(len(row)))
        return Record(line, self.lineParser.lineNumber, row[0], row[1], row[2], int(row[3]), int(row[4]), row[5], row[6], row[7], self.__parseAttrs(row[8]))

    def reader(self):
        "Generator over records"
        try:
            for line in self.lineParser.reader():
                yield self.__parseRecord(line)
        finally:
            self.close()
