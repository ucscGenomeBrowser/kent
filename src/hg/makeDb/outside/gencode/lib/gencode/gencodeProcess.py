"""
functions and classes for processing gencode data files.
"""
import os
import sys
import re
from pycbio.sys import fileOps

class LiftError(Exception):
    pass

class LiftOverUnmapped(object):
    "parsed liftOver unmapped file"
    def __init__(self, dataFormat, unmappedOutFile):
        isBed = dataFormat.startswith("bed")
        self.droppedSeqs = set()  # chroms that are not mapped
        self.otherProblems = []  # tuples of (description, record)
        lines = fileOps.readFileLines(unmappedOutFile)
        for i in range(0, len(lines), 2):
            self.__parseEntry(lines[i], isBed, lines[i + 1].split("\t"))

    def __parseEntry(self, description, isBed, rec):
        # entries line:
        #   # Boundary problem: need 2, got 1, diff 1, mapped 0.5
        #   ENST00000619954.1	HSCHR12_4_CTG2_1	...

        # The "Delete in new" message is preceded by a space for genePred and but not BED
        if re.match("^# *Deleted in new", description):
            self.droppedSeqs.add(rec[0 if isBed else 1])
        elif description.startswith("#"):
            self.otherProblems.append((description, rec))
        else:
            raise Exception("invalid description line in liftOver unmapped file: " + description)

    def report(self):
        "warning on skip sequences, error on other problems"
        if len(self.droppedSeqs) > 0:
            sys.stderr.write("Warning: annotations not mapped for these chromosomes, the maybe new patches/alt sequence not in UCSC: " + " ".join(sorted(self.droppedSeqs)) + "\n")
        if len(self.otherProblems) > 0:
            for probInfo in self.otherProblems:
                sys.stderr.write("Error: " + probInfo[0] + "\n")
                sys.stderr.write("       " + "\t".join(probInfo[1]) + "\n")
            raise LiftError("Errors occurred while lifting")

class GencodeLiftOver(object):
    def __init__(self, dataFormat, liftFile, unmappedOutFile=None):
        """dataFormat should be 'genePred' or 'bedPlus=N'"""
        self.dataFormat = dataFormat
        self.liftFile = liftFile
        self.unmappedOutFile = unmappedOutFile
        self.unmappedOutFileTmp = None
        if self.unmappedOutFile is None:
            self.unmappedOutFileTmp = self.unmappedOutFile = fileOps.tmpFileGet("gencode.dropped")

    def getLiftoverCmd(self, stdin="stdin", stdout="stdout"):
        """get command tuple for lift"""
        cmd = ["liftOver", "-" + self.dataFormat]
        if self.dataFormat.startswith("bed"):
            cmd += ["-tab"]
        cmd += [stdin, self.liftFile, stdout, self.unmappedOutFile]
        return tuple(cmd)

    def getLiftOverUnmappedInfo(self):
        return LiftOverUnmapped(self.dataFormat, self.unmappedOutFile)

    def removeTmp(self):
        "if a tmp file was created, drop it"
        if (self.unmappedOutFileTmp is not None) and os.path.exists(self.unmappedOutFileTmp):
            os.unlink(self.unmappedOutFileTmp)

def getEditIdCmd(column0):
    """return awk command to edit ids in GTF file"""
    return ('awk', '-v', 'FS=\\t', '-v', 'OFS=\\t', '-v', 'column={}'.format(column0 + 1),
            '{sub("^ENSTR", "ENST0", $column); sub("^ENSGR", "ENSG0", $column); sub("_PAR_Y", "", $column); print $0}')
