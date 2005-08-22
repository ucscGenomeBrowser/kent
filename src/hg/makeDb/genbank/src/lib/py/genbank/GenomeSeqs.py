"module to access information about genome sequences"

import os, glob
from genbank.fileOps import prLine, prRow
from genbank import procOps

class GenomeSeq(object):
    "information about a genome sequence"
    def __init__(self, path, id, size):
        self.path = path
        self.id = id
        self.size = size
        # regions without gaps, starts as whole chr, maybe rebuilt from lift
        self.regions = [(0, size)]
    
class GenomeSeqs(dict):
    "table of genome sequences and sizes for a database"

    def __init__(self, genomeGlob, skipChroms):
        """expand genomeGlob and add all sequences. If skipChroms is not None,
        it is is a list of chrs to ignore"""

        self.genomeDb = None
        if skipChroms != None:
            self.skipChroms = frozenset(skipChroms)
        else:
            self.skipChroms = frozenset()
        paths = glob.glob(genomeGlob)
        if len(paths) == 0:
            raise Exception("no files matching: " + genomeGlob)
        if (len(paths) == 1) and paths[0].endswith(".2bit"):
            self._addTwoBit(paths[0])
        else:
            self._addNibs(paths)

    def _addNibs(self, paths):
        "add nib sequences to object"
        # /cluster/data/hg17/nib/chrX.nib	chrX	154824264
        self.genomeDb = os.path.dirname(paths[0])
        lines = procOps.callProcLines(["nibSize"] + paths)
        for line in lines:
            row = line.split("\t")
            if row[1] not in self.skipChroms:
                self[row[1]] = GenomeSeq(row[0], row[1], int(row[2]))

    def _addTwoBit(self, path):
        "add twoBit sequences to object"
        self.genomeDb = path[0]
        # chrX	154824264
        lines = procOps.callProcLines(["twoBitInfo", path, "stdout"])
        for line in lines:
            row = line.split("\t")
            if row[0] not in self.skipChroms:
                self[row[0]] = GenomeSeq(path, row[0], int(row[1]))

    def _loadLift(self, liftFile):
        "load lift into dict of lists of (start end), ensuring they are sorted"
        fh = open(liftFile)
        lifts = {}
        # offset oldName oldSize newName newSize strand
        for line in fh:
            row = line[0:-1].split("\t")
            if (row[1] != "gap") and (row[3] not in self.skipChroms):
                start = int(row[0])
                end = start + int(row[2])
                if not row[3] in lifts:
                    lifts[row[3]] = []
                lifts[row[3]].append((start, end))
        fh.close
        for l in lifts.itervalues():
            l.sort()
        return lifts

    def _addSeqRegions(self, seq, lifts):
        "add ungapped regions for a sequence"
        seq.regions = []
        start = lifts[0][0]
        end = lifts[0][1]
        for lift in lifts[1:]:
            if (lift[0] - end) > 0:
                seq.regions.append((start, end))
                start = lift[0]
            end = lift[1]
        seq.regions.append((start, end))

    def addUnGappedRegions(self, liftFile):
        "define regions without gaps from a lift file"
        lifts = self._loadLift(liftFile)
        for id in lifts.iterkeys():
            self._addSeqRegions(self[id], lifts[id])
    
    def dump(self, fh):
        "print contents for debugging purposes"
        ids = self.keys()
        ids.sort()
        for id in ids:
            seq = self[id]
            prRow(fh, (seq.id, seq.size, seq.path))
            if seq.regions != None:
                for reg in seq.regions:
                    fh.write("\t")
                    prRow(fh, reg)

