"Module to store information about genome sequences"

import os, glob
from genbank.fileOps import prLine, prRow
from genbank import procOps

class GenomeSeq(object):
    "information about a genome sequence"
    __slots__ = ("genomeSeqs", "path", "id", "size", "unplaced", "regions")
    def __init__(self, genomeSeqs, path, id, size):
        self.genomeSeqs = genomeSeqs
        self.path = path
        self.id = id
        self.size = size
        self.unplaced = False  # unplaced seq, gaps not spanned if set.
        # regions without gaps, starts as whole chr, maybe rebuilt from lift
        self.regions = [(0, size)]
    
class GenomeSeqs(dict):
    "table of genome sequences and sizes for a database"

    def __init__(self, db, genomeFileSpec):
        """Build from all sequences in a genome.
        - db - genome database or other name used to identify the genome.
        - genomeFileSpec - either a glob pattern or file specification for 2bit
          or nib genome seq files.
        """

        self.db = db
        paths = glob.glob(genomeFileSpec)
        if len(paths) == 0:
            raise Exception("no files matching: " + genomeFileSpec)
        if (len(paths) == 1) and paths[0].endswith(".2bit"):
            self.__addTwoBit(paths[0])
        else:
            self.__addNibs(paths)

    def __addNibs(self, paths):
        "add nib sequences to object"
        # /cluster/data/hg17/nib/chrX.nib	chrX	154824264
        lines = procOps.callProcLines(["nibSize"] + paths)
        for line in lines:
            row = line.split("\t")
            self[row[1]] = GenomeSeq(self, row[0], row[1], int(row[2]))

    def __addTwoBit(self, path):
        "add twoBit sequences to object"
        self.genomeDb = path[0]
        # chrX	154824264
        lines = procOps.callProcLines(["twoBitInfo", path, "stdout"])
        for line in lines:
            row = line.split("\t")
            self[row[0]] = GenomeSeq(self, path, row[0], int(row[1]))

    def __loadLift(self, liftFile):
        "load lift into dict of lists of (start end), ensuring they are sorted"
        fh = open(liftFile)
        lifts = {}
        # offset oldName oldSize newName newSize strand
        for line in fh:
            row = line[0:-1].split("\t")
            if row[1] != "gap":
                start = int(row[0])
                end = start + int(row[2])
                if not row[3] in lifts:
                    lifts[row[3]] = []
                lifts[row[3]].append((start, end))
        fh.close
        for l in lifts.itervalues():
            l.sort()
        return lifts

    def __addSeqRegionsFromLifts(self, seq, lifts):
        "add ungapped regions for a sequence"
        seq.regions = []
        start = lifts[0][0]
        end = lifts[0][1]
        for lift in lifts[1:]:
            if ((lift[0] - end) > 0) or seq.unplaced:
                seq.regions.append((start, end))
                start = lift[0]
            end = lift[1]
        seq.regions.append((start, end))

    def defineSeqRegionsFromLifts(self, liftFile):
        """define regions without gaps from a lift file.  If a sequence
        is flagged as unplaced, adjacent lift entries are not joined"""
        lifts = self.__loadLift(liftFile)
        for id in lifts.iterkeys():
            self.__addSeqRegionsFromLifts(self[id], lifts[id])
    
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

