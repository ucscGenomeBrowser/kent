import subprocess
import re

from ucscGb.qa import qaUtils
from ucscGb.qa.tables.tableQa import TableQa

shortLabelLimit = 17
longLabelLimit = 80
genbankTableListDev = "/cluster/data/genbank/etc/genbank.tbls"
genbankTableListBeta = "/genbank/etc/genbank.tbls"

# Read the list of genbank tables once, for __positionalTblCheck()
try:
    with open(genbankTableListDev, "r") as f:
        raw = f.readlines()
except IOError:
    try:
        with open(genbankTableListBeta, "r") as f:
            raw = f.readlines()
    except IOError:
        raise Exception("Cannot open either " + genbankTableListDev + " or " + genbankTableListBeta)
genbankRegexes = [name.strip() for name in raw]


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

    def __getParent(self, track):
        """Returns name of parent.  Looks for either 'parent' or 'subtrack' setting in trackDb."""
        parent = self.__getAttributeForTrack("parent", track)
        if not parent:
            parent = self.__getAttributeForTrack("subTrack", track)
        if parent:
            parent = parent.split()[0] # get rid of the "on" or "off" setting if present
        return parent

    def __getLabels(self, track, shortList, longList):
        """Adds short and long labels for this track and its parents to shortList and longList.
        Excludes 'view' labels."""
        view = self.__getAttributeForTrack("view", track)
        if not view:
            shortList.append(self.__getAttributeForTrack("shortLabel", track))
            longList.append(self.__getAttributeForTrack("longLabel", track))
        parent = self.__getParent(track)
        if parent:
            self.__getLabels(parent, shortList, longList)

    def checkLabelLengths(self):
        """Checks that short and long labels (for this track + parents) are shorter than limits.
        Writes labels to reporter. Ignores 'view' labels. Makes an exception for shortLabel 
        '$o_Organism Chain/Net'."""
        self.reporter.beginStep(self.db, self.table, "checking label lengths:")
        self.reporter.writeStepInfo()
        shortLabels = []
        longLabels = []
        # need to add the first labels regardless of 'view' setting, which is inherited
        shortLabels.append(self.__getAttributeForTrack("shortLabel", self.table))
        longLabels.append(self.__getAttributeForTrack("longLabel", self.table))
        parent = self.__getParent(self.table)
        if parent:
            self.__getLabels(parent, shortLabels, longLabels)
        for label in shortLabels:
            self.reporter.writeLine('"' + label + '"')
            if len(label) > shortLabelLimit and label != "$o_Organism Chain/Net":
                self.recordError()
            else:
                self.recordPass()
        for label in longLabels:
            self.reporter.writeLine('"' + label + '"')
            if len(label) > longLabelLimit:
                self.recordeError()
            else:
                self.recordPass()
        self.reporter.endStep()

    def __isGenbankTable(self):
        for regex in genbankRegexes:
            if re.match(regex, self.table):
                return True

    def __positionalTblCheck(self):
        """Runs positionalTblCheck program on this table. Excludes GenBank tables."""
        if not self.__isGenbankTable():
            self.reporter.beginStep(self.db, self.table, "positionalTblCheck")
            command = ["positionalTblCheck", self.db, self.table]
            self.reporter.writeCommand(command)
            p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
            p.wait()
            if p.returncode:
                self.recordError()
            else:
                self.recordPass()
            self.reporter.endStep()

    def __checkTableCoords(self):
        """Runs checkTableCoords program on this table."""
        self.reporter.beginStep(self.db, self.table, "checkTableCoords")
        command = ["checkTableCoords", self.db, self.table]
        self.reporter.writeCommand(command)
        p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
        p.wait()
        if p.returncode:
            self.recordError()
        else:
            self.recordPass()
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
