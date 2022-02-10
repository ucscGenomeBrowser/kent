"""
shared code for GFF3 and GTF parsing.
"""
import re
from pycbio.sys import fileOps

class ParseException(Exception):
    "indicates a parser error that is report, but allows checking to continue"
    def __init__(self, gxfParser, msg):
        Exception.__init__(self, "Error: " + gxfParser.gxfFile + ":" + str(gxfParser.lineNum) + ": " + msg)

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

    def __advanceLine(self):
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

    def __ignored(self, line):
        return self.ignoredRe.search(line) is not None

    def reader(self):
        "Generator over record lines"
        try:
            while self.__advanceLine():
                if self.line.startswith("##"):
                    self.metas.append(self.line)
                elif not self.__ignored(self.line):
                    yield self.line
        finally:
            self.close()
