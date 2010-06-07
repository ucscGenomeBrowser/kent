"objects to manage genbank release and update infomation"
#
# $Id: GbIndex.py,v 1.3 2006/04/07 14:10:26 markd Exp $
#

# FIXME: tmp hack
import sys
sys.path.append("..")

import glob
import os.path
from genbank.fileOps import prLine, prStrs

# constants used for source databases
GenBank = "genbank"
RefSeq = "refseq"

# data processing steps
Processed = "processed"
Aligned = "aligned"

# cdnaType
MRNA = "mrna"
EST = "est"

# cdnaTypes valid for a srcDb
SrcDbCdnaTypes = {GenBank: set((MRNA, EST)),
                  RefSeq: set((MRNA,))}


# orgCats
Native = "native"
Xeno = "xeno"

def globSort(pat):
    "do a glob and sort the results"
    paths = glob.glob(pat)
    paths.sort()
    return paths

class GbProcessedPart(object):
    "a partition of the processed data in the update"
    __slots__ = ("update", "cdnaType", "accPrefix", "getAlignedPart", "__str__")

    def __init__(self, update, cdnaType, accPrefix=None):
        self.update = update
        self.cdnaType = cdnaType
        self.accPrefix = accPrefix

    def getGbIdx(self):
        "get path to gbidx file for this partition"
        gbIdx = self.update.rel.relProcDir + "/" + self.update + "/" + self.cdnaType
        if self.accPrefix != None:
            gbIdx += "." + self.accPrefix
        gbIdx += ".gbidx"
        return gbIdx

    def getAlignedPart(self):
        "get the GbAlignedPart object associated with this object, or None"
        return self.update.getProcAlignedPart(self)

    def __str__(self):
        if self.accPrefix != None:
            return self.cdnaType + "." + self.accPrefix
        else:
            return self.cdnaType

class GbAlignedPart(object):
    "a partition of the aligned data in the update"
    __slots__ = ("cdnaType", "orgCats", "accPrefix", "__str__")

    # orgCats are kept as a set, since cDNAs are obtained from
    # the same file and then split

    def __init__(self, cdnaType, accPrefix=None):
        self.cdnaType = cdnaType
        self.orgCats = set()
        self.accPrefix = accPrefix

    def __str__(self):
        if self.accPrefix != None:
            return self.cdnaType + ".{" + ",".join(self.orgCats) + "}." + self.accPrefix
        else:
            return self.cdnaType + ".{" + ",".join(self.orgCats) + "}"

class GbUpdate(str):
    "information about an update"

    def __new__(cls, rel, upd):
        self = str.__new__(cls, upd)
        self.rel = rel

        # vectors of data partitions, loaded on demand. Only one database's
        # aligned partitions maybe loaded at a given time.  Aligned map is
        # index by integer constructed from parameters.
        self.procParts = None
        self.alignDb = None
        self.alignParts = None
        self.alignMap = None
        return self

    def _partIndex(self, cdnaType, accPrefix):
        "generate an index for an aligned partition"
        # Note: this could be done with a lookup table
        if accPrefix == None:
            idx = 0
        else:
            idx = (ord(accPrefix[0]) << 8) | ord(accPrefix[1])
        idx <<= 1  # room for cdnaType
        if cdnaType == EST:
            idx |= 1
        return idx

    def loadProcessedParts(self):
        "load lists of existing processed partitions for the updated, if not already done"
        if self.procParts == None:
            self.procParts = []
            procDir = "data/processed/" + self.rel + "/" + self
            for gbIdx in globSort(procDir + "/mrna.gbidx"):
                self.procParts.append(GbProcessedPart(self, MRNA))
            for gbIdx in globSort(procDir + "/est.*.gbidx"):
                names = os.path.basename(gbIdx).split(".")
                self.procParts.append(GbProcessedPart(self, EST, intern(names[1])))

    def _addAlignedPart(self, cdnaType, orgCat, accPrefix=None):
        # create new or add orgCat
        idx = self._partIndex(cdnaType, accPrefix)
        if idx in self.alignMap:
            part = self.alignMap[idx]
        else:
            part = GbAlignedPart(cdnaType, accPrefix)
            self.alignParts.append(part)
            self.alignMap[idx] = part
        part.orgCats.add(orgCat)

    def loadAlignedParts(self, db):
        "load lists of existing aligned partitions for the updated, if not already done"
        if self.alignDb != db:
            self.alignDb = db
            self.alignParts = []
            self.alignMap = {}
            alnDir = "data/aligned/" + self.rel + "/" + db + "/" + self
            for alIdx in globSort(alnDir + "/mrna.*.alidx"):
                names = os.path.basename(alIdx).split(".")
                self._addAlignedPart(MRNA, intern(names[1]))
            for alIdx in globSort(alnDir + "/est.*.*.alidx"):
                names = os.path.basename(alIdx).split(".")
                self._addAlignedPart(EST, intern(names[2]), intern(names[1]))

    def getProcAlignedPart(self, procPart):
        """get GbAlignedPart object corresponding to procPart, and orgCat
        or None if not found"""
        return self.alignMap.get(self._partIndex(procPart.cdnaType, procPart.accPrefix))
            
    def dump(self, fh, indent="\t"):
        prLine(fh, indent, self)
        indent2 = indent + "\t"
        if self.procParts != None:
            prStrs(fh, indent2, "processed:")
            for p in self.procParts:
                prStrs(fh, " ", p)
            fh.write("\n")
        if self.alignDb != None:
            prStrs(fh, indent2, "aligned:")
            for p in self.alignParts:
                prStrs(fh, " ", p)
            fh.write("\n")
            

class GbRelease(str):
    "information about a release of genbank or refseq."
    def __new__(cls, srcDb, rel, relProcDir):
        self = str.__new__(cls, rel)
        self.srcDb = srcDb
        self.version = float(rel.split(".", 1)[1])
        self.cdnaTypes = SrcDbCdnaTypes[srcDb]  # valid cDNA types
        self.relProcDir = relProcDir
        self.updates = None  # None indicates no update search has been done
        self.alignDb = None  # aligned database currently loaded
        return self

    def _addUpdates(self, updateGlob):
        updDirs = globSort(updateGlob)
        updDirs.sort()
        for updDir in updDirs:
            if os.access(updDir, os.X_OK):
                upd = GbUpdate(self, os.path.basename(updDir))
                self.updates.append(upd)
                upd.loadProcessedParts()
        
    def loadUpdates(self):
        "load list of updates, if not already loaded"
        if (self.updates == None):
            # add in assending order
            self.updates = []
            self._addUpdates(self.relProcDir + "/full")
            self._addUpdates(self.relProcDir + "/daily.*")

    def loadAlignedParts(self, db):
        self.loadUpdates()
        if self.alignDb != db:
            self.alignDb = db
            for upd in self.updates:
                upd.loadAlignedParts(db)

    def __cmp__(self, other):
        "comparison for releases, sorts newest to oldest"
        if other == None:
            return 1
        assert(self.srcDb == other.srcDb)
        if self.srcDb == RefSeq:
            # special handling for migration from faked to real refseq releases
            # fake release contains real number, real contains integer.
            selfReal = (self.count(".") == 1)
            otherReal = (other.count(".") == 1)
            if selfReal and not otherReal:
                return 1
            if otherReal and not selfReal:
                return -1
            # all real or all fake, do normal compare
        return -1 *cmp(self.version, other.version)

    def dump(self, fh, indent="\t"):
        "dump for debugging purposes"
        prLine(fh, indent, self, "  ", self.version)
        if self.updates == None:
            prLine(fh, indent, "\t", "updates not loaded" )
        else:
            indent2 = indent + "\t"
            for up in self.updates:
                up.dump(fh, indent2)
            

class GbSrcDb(str):
    "information about genbank or refseq"
    def __new__(cls, gbIndex, srcDbName):
        self = str.__new__(cls, srcDbName)
        self.gbIndex = gbIndex
        self.rels = []
        
        # get list of releases from processed dir, ignoring
        # unlistable directories
        for relProcDir in globSort("data/processed/" + self + ".*"):
            if os.access(relProcDir, os.X_OK):
                self.rels.append(GbRelease(self, os.path.basename(relProcDir), relProcDir))
        self.rels.sort(cmp)
        return self

    def getLatestRel(self):
        "get the lastest release, or None if no release.  Load updates if needed"
        if len(self.rels) == 0:
            return None
        else:
            self.rels[0].loadUpdates()
            return self.rels[0]

    def dump(self, fh):
        "dump for debugging purposes"
        prLine(fh, self)
        for rel in self.rels:
            rel.dump(fh)

class GbIndex(dict):
    """directory of all genbank/refseqs releases and updates, contains
    GbSrcDb objects"""

    def __init__(self):
        self[GenBank] = GbSrcDb(self, GenBank)
        self[RefSeq] = GbSrcDb(self, RefSeq)

    def dump(self, fh):
        "dump for debugging purposes"
        self[GenBank].dump(fh)
        self[RefSeq].dump(fh)


if __name__ == "__main__":
    # quick test
    os.chdir("/cluster/data/genbank")
    gbi = GbIndex()
    rel = gbi[GenBank].getLatestRel()
    rel.loadAlignedParts("hg17")
    rel.dump(sys.stdout)
