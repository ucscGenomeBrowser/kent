import subprocess

from ucscGb.qa import qaUtils
from ucscGb.qa.tables.tableQa import TableQa

genbankTableListDev = "/cluster/data/genbank/etc/genbank.tbls"
genbankTableListBeta = "/genbank/etc/genbank.tbls"
shortLabelLimit = 17
longLabelLimit = 80

class PositionalQa(TableQa):
    """
    A positional table.
    """

    def __getAttributeForTrack(self, attribute, track):
        """Uses tdbQuery to get an attribute where track or table == 'track' and removes label."""
        cmd = ["tdbQuery", "select " + attribute + " from " + self.db + " where table='" + track +
               "' or track='" + track + "'"]
        cmdout, cmderr = qaUtils.runCommand(cmd)
        return cmdout.strip(attribute).strip()

    def __getLabels(self, track, shortList, longList):
        """Adds short and long labels for this track and its parents to shortList and longList."""
        shortList.append(self.__getAttributeForTrack("shortLabel", track))
        longList.append(self.__getAttributeForTrack("longLabel", track))
        parent = self.__getAttributeForTrack("parent", track)
        if parent:
            parent = parent.split()[0] # get rid of the "on" or "off" setting if present
            self.__getLabels(parent, shortList, longList)

    def checkLabelLengths(self):
        """Checks that short and long labels (for this track + parents) are shorter than limits.
        Writes labels to reporter."""
        self.reporter.beginStep(self.db, self.table, "checking label lengths")
        self.reporter.writeStepInfo()
        shortLabels = []
        longLabels = []
        self.__getLabels(self.table, shortLabels, longLabels)
        for label in shortLabels:
            error = False
            self.reporter.writeLine("  " + label)
            if len(label) > shortLabelLimit and label != "$o_Organism Chain/Net":
                error = True
            self.recordPassOrError(error)
        for label in longLabels:
            error = False
            self.reporter.writeLine("  " + label)
            if len(label) > longLabelLimit:
                error = True
            self.recordPassOrError(error)
        self.reporter.endStep()

    def __positionalTblCheck(self):
        """Runs positionalTblCheck program on this table. Excludes GenBank tables."""
        #TODO: make it exclude genbank tables
        self.reporter.beginStep(self.db, self.table, "positionalTblCheck")
        command = ["positionalTblCheck", self.db, self.table]
        self.reporter.writeCommand(command)
        p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
        p.wait()
        self.recordPassOrError(p.returncode)
        self.reporter.endStep()

    def __checkTableCoords(self):
        """Runs checkTableCoords program on this table."""
        self.reporter.beginStep(self.db, self.table, "checkTableCoords")
        command = ["checkTableCoords", self.db, self.table]
        self.reporter.writeCommand(command)
        p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
        p.wait()
        self.recordPassOrError(p.returncode)
        self.reporter.endStep()

    def __chromosomeCoverage(self):
        """Returns a chrom counts object with the number of items per chromosome in this table."""
        pass

    def __featureBits(self):
        """Adds featureBits output (both regular and overlap w/ gap) to sumRow."""
        fbCommand = ["featureBits", "-countGaps", self.db, self.table]
        fbOut, fbErr = qaUtils.runCommand(fbCommand)
        # normal featureBits output actually goes to stderr
        self.sumRow.setFeatureBits(fbErr.rstrip("in intersection\n"))
        fbGapCommand = ["featureBits", "-countGaps", self.db, self.table, "gap"]
        fbGapOut, fbGapErr = qaUtils.runCommand(fbGapCommand)
        self.sumRow.setFeatureBitsGaps(fbGapErr.rstrip("in intersection\n"))

    def validate(self):
        """Adds positional-table-specific checks to basic table checks."""
        super(PositionalQa, self).validate()
        self.checkLabelLengths()
        self.__positionalTblCheck()
        self.__checkTableCoords()

    def statistics(self):
        super(PositionalQa, self).statistics()
        self.__featureBits()
