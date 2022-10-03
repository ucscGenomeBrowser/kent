"""
shared code for GFF3 and GTF parsing.
"""
import re
from pycbio.sys import fileOps
from gencode.biotypes import BioType
from gencode.gencodeTags import GencodeTag

class ParseException(Exception):
    "indicates a parser error that is report, but allows checking to continue"
    def __init__(self, gxfParser, msg):
        Exception.__init__(self, "Error: " + gxfParser.gxfFile + ":" + str(gxfParser.lineNumber) + ": " + msg)

class GxfParser(object):
    """common code shared between GTF and GFF3 parsers"""

    def __init__(self, gxfFile=None, gxfFh=None):
        self.gxfFile = gxfFile if gxfFile is not None else "<unknown>"
        self.openedFile = (gxfFh is None)
        self.fh = fileOps.opengz(gxfFile) if gxfFh is None else gxfFh
        self.line = None
        self.lineNumber = 0
        self.metas = []

    def parseException(self, msg):
        "return a parse exception"
        return ParseException(self, msg)

    def _advanceLine(self):
        "sets object, return True or False"
        self.line = self.fh.readline()
        if len(self.line) == 0:
            self.line = None
            return False
        else:
            self.lineNumber += 1
            self.line = self.line[0:-1]
            return True

    def close(self):
        if (self.fh is not None) and self.openedFile:
            self.fh.close()
        self.fh = None

    ignoredRe = re.compile("(^[ ]*$)|(^[ ]*#.*$)")  # spaces or comment line

    def _ignored(self, line):
        return self.ignoredRe.search(line) is not None

    def reader(self):
        "Generator over record lines"
        try:
            while self._advanceLine():
                if self.line.startswith("##"):
                    self.metas.append(self.line)
                elif not self._ignored(self.line):
                    yield self.line
        finally:
            self.close()


class GxfValidateException(Exception):
    "indicates a validation error"
    pass

class GxfRecordValidator:
    """Validate GENCODE GxF records checking non-syntactic attributes.
    This only reports the first occurrence of each error and allows
    reporting all types of errors rather than the first one"""

    def __init__(self, errFh):
        self.invalidGeneTypes = set()
        self.invalidTranscriptTypes = set()
        self.invalidTags = set()
        self.errorCnt = 0
        self.errFh = errFh

    def _checkValue(self, vtype, vname, value, rec, errorSet):
        try:
            vtype(value)
        except ValueError:
            if value not in errorSet:
                print(f"Error: unknown {vname} '{value}', first occurrence on line {rec.lineNumber} update gencode/biotypes.py or fix data:\n{rec.line}", file=self.errFh)
                errorSet.add(value)
            self.errorCnt += 1

    def validate(self, rec):
        self._checkValue(BioType, "gene_type", rec.attributes.gene_type, rec, self.invalidGeneTypes)
        if rec.feature != "gene":
            self._checkValue(BioType, "transcript_type", rec.attributes.transcript_type, rec, self.invalidGeneTypes)
        for tag in rec.attributes.tags:
            self._checkValue(GencodeTag, "tag", tag, rec, self.invalidTags)

    def reportErrors(self):
        if self.errorCnt > 0:
            raise GxfValidateException(f"found {self.errorCnt} attribute errors")
