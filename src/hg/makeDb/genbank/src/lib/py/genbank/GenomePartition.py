import fnmatch
from genbank.fileOps import prRowv
from genbank.GenomeSeqs import GenomeSeqs

class UnplacedChroms(object):
    "object used to determine if a chromosome is unplaced"

    def __init__(self, unSpecs):
        self.unplaced = set()
        self.pats = []

        # separate into patterns and simple names for performance reasons
        for spec in unSpecs:
            if ("*" in spec) or ("?" in spec) or ("[" in spec):
                self.pats.append(spec)
            else:
                self.unplaced.add(spec)

        if len(self.unplaced) == 0:
            self.unplaced = None
        if len(self.pats) == 0:
            self.pats = None

    def __contains__(self, chrom):
        "determine if a chroms is unplaced"
        if self.unplaced != None:
            if chrom in self.unplaced:
                return True
        if self.pats != None:
            for pat in self.pats:
                if fnmatch.fnmatchcase(chrom, pat):
                    return True
        return False

    def flagSeqs(self, seqs):
        "flag unplaced sequences in the genome"
        for seq in seqs.itervalues():
            if seq.id in self:
                seq.unplaced = True

class GenomeWindow(object):
    "information about a single genome window"
    __slots__ = ("seq", "start", "end", "size", "isFullSeq", "getSpec")

    def __init__(self, seq, start, end):
        assert(start < end)
        self.seq = seq
        self.start = start
        self.end = end
        self.size = end - start
        self.isFullSeq = (self.size == seq.size)

    def getSpec(self):
        "get specification for gbBlat (includes size if subrange)"
        if self.isFullSeq:
            return self.seq.id
        else:
            return self.seq.id + ":" + str(self.seq.size) + ":" + str(self.start) + "-" + str(self.end)

    def __str__(self):
        return self.getSpec()

    def dump(self, fh):
        "print object for debugging purposes"
        prRowv(fh, self.seq.id, self.start, self.end)

class GenomeWindows(list):
    """Class to construct overlapping windows over sequences in a genome.
    contains ordered list of GenomeWindow objects"""

    def __init__(self, genomeSeqs, winSize, overlap, maxGap,
                 minUnplacedSize=0):
        """
        - winSize - Size of genome alignment windows, in bases. The genome is
          partitioned in segments of no more than this sizes for alignment.
        - overlap - Number of bases of overlap in the alignment windows.
        - maxGap - Gaps no larger than this value are contained within a window rather
          then starting a new window.  This allows gaps within introns.
        - minUnplacedSize - Skip unplaced sequences smaller than this size.
        """
        self.winSize = winSize
        self.overlap = overlap
        self.maxGap = maxGap
        self.minUnplacedSize = minUnplacedSize
        if (self.winSize <= 0):
            raise Exception("window size must be > 0")
        if (self.overlap < 0):
            raise Exception("window overlap must be >= 0")
        if (self.winSize <= self.overlap):
            raise Exception("window size must be > overlap")

        for seq in genomeSeqs.itervalues():
            if seq.unplaced:
                self.__partitionUnplaced(seq)
            else:
                self.__partitionSeq(seq)
                
    def __partitionSeq(self, seq):
        "partition a sequence"
        start = seq.regions[0][0]
        end = seq.regions[0][1]
        for reg in seq.regions[1:]:
            if (reg[0] - end) >= self.maxGap:
                self.__partitionRange(seq, start, end)
                start = reg[0]
            end = reg[1]
        self.__partitionRange(seq, start, end)

    def __partitionUnplaced(self, seq):
        "partition a pseudo-chromosome containing unplaced sequences"
        # assumed lift file divded chromosome into unplaced sequences
        for reg in seq.regions:
            if (reg[1] - reg[0]) >= self.minUnplacedSize:
                self.append(GenomeWindow(seq, reg[0], reg[1]))

    def __partitionRange(self, seq, start, end):
        "partition a range into windows"
        assert(start < end)
        winStart = start
        while winStart < end:
            winEnd = winStart + self.winSize
            if winEnd+self.overlap >= end:
                # past end or next overlap will take us to end
                winEnd = end
            self.append(GenomeWindow(seq, winStart, winEnd))
            # advance window with overlap.
            winStart += (self.winSize - self.overlap)

    def dump(self, fh):
        "print object for debugging purposes"
        for win in self:
            win.dump(fh)            

class GenomePartition(object):
    "object to store info about genome and partition it into windows"

    def __init__(self, db, genomeFileSpec, winSize, overlap, maxGap,
                 minUnplacedSize=0, liftFile=None, unplacedChroms=None):
        self.db = db
        self.seqs = GenomeSeqs(db, genomeFileSpec)

        # flag unplaced chroms before loading lift, as it changes
        # partitioning
        if unplacedChroms != None:
            unplacedChroms.flagSeqs(self.seqs)

        if liftFile != None:
            self.seqs.defineSeqRegionsFromLifts(liftFile)
        self.windows = GenomeWindows(self.seqs, winSize, overlap, maxGap,
                                     minUnplacedSize)

    def dump(self, fh):
        "print info about object"
        fh.write("=========== GENOME REGIONS ===========\n")
        self.seqs.dump(fh)
        fh.write("=========== WINDOWS ===========\n")
        self.windows.dump(fh)
        fh.flush()

