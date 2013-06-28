import subprocess
import re
import math

from ucscGb.qa import qaUtils
from ucscGb.qa.tables.tableQa import TableQa

shortLabelLimit = 17
longLabelLimit = 80
genbankTableListDev = "/cluster/data/genbank/etc/genbank.tbls"
genbankTableListBeta = "/genbank/etc/genbank.tbls"

# group the types of positional tables by the name of the column that contains the chrom
# couldn't find any existing chromGraph tables, so left it out for now
chromTypes = frozenset(['bed', 'genePred', 'axt', 'clonePos', 'ctgPos', 'expRatio', 'maf', 'sample',
                        'wigMaf', 'wig', 'bedGraph', 'factorSource', 'bedDetail', 'Ld2',
                        'bed5FloatScore', 'bedRnaElements', 'broadPeak', 'gvf', 'narrowPeak',
                        'peptideMapping', 'pgSnp'])
tNameTypes = frozenset(['chain', 'psl', 'altGraphX', 'netAlign'])
genoNameTypes = frozenset(['rmsk'])


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
                self.recordError()
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

    def __getChromCol(self):
        """Returns the name of the column that contains the chrom name, based on track type."""
        if self.tableType in chromTypes:
            return 'chrom'
        elif self.tableType in genoNameTypes:
            return 'genoName'
        elif self.tableType in tNameTypes:
            return 'tName'
        else:
            raise Exception("Can't find column name for track type " + self.tableType)

    def __getChromCountsFromDatabase(self):
        """Returns a list of ['chrom', 'size (bases)', 'count', 'count/megabase'] lists for table"""
        chromCol = self.__getChromCol()
        # first half of the UNION below gets rows that do not appear in table at all 
        query = "(SELECT chrom, size, 0 as count FROM chromInfo WHERE chrom NOT IN" +\
            " (select distinct " + chromCol + " from " + self.table + "))" +\
            " UNION" +\
            " (SELECT inf.chrom, inf.size, count(tab." + chromCol + ") as count" +\
            " FROM chromInfo as inf, " + self.table + " as tab" +\
            " WHERE inf.chrom=tab." + chromCol + " GROUP BY chrom)"
        hgsqlOut = qaUtils.callHgsql(self.db, query)
        rows = hgsqlOut.split("\n") # makes list of strings with tabs embedded
        rows.remove('') # get rid of the empty list item created by the final newline
        rowsList = []
        for row in rows:
            rowsList.append(row.split("\t")) # result is list of ['chrom', 'size', 'count']
        for row in rowsList:
            # calculate count/size (in megabases) as floats, put in rowsList[3]
            row.append(float(row[2])*1000000/float(row[1]))
        return rowsList

    def __getMeanAndSD(self, counts):
        """Returns mean and standard deviation for 4th column of a list of lists. """
        n = len(counts)
        mean = sum([row[3] for row in counts])/n
        diffs = [row[3] - mean for row in counts]
        diffsSquared = [x*x for x in diffs]
        variance = sum(diffsSquared)/n
        SD = math.sqrt(variance)
        return mean, SD

    def __writeZerosInfo(self, zeros, maxToShow):
        """Writes info for biggest chroms with count of zero to reporter."""
        zerosTotal = len(zeros)
        zerosToShow = min(zerosTotal, maxToShow)
        self.reporter.writeLine("\ntotal chroms with count of zero: " + str(zerosTotal))
        if zerosTotal > 0:
            zeros.sort(key=lambda size: int(size[1]), reverse=True)
            self.reporter.writeLine("the " + str(zerosToShow) + " biggest:")
            for i in range(zerosToShow):
                self.reporter.writeLine('[%s, %s, %s, %g]' % tuple(zeros[i]))

    def __writeUnderLimitInfo(self, countsUnderLimit, maxToShow):
        """Writes info for chroms with lowest counts."""
        countsUnderTotal = len(countsUnderLimit)
        lowToShow = min(countsUnderTotal, 5)
        self.reporter.writeLine("\ntotal chroms below lower limit: " + str(countsUnderTotal))
        if countsUnderTotal > 0:
            self.reporter.writeLine("\nthe " + str(lowToShow) + " with lowest counts per size: ")
            countsUnderLimit.sort(key=lambda countPer: float(countPer[3]))
            for i in range(lowToShow):
                self.reporter.writeLine('[%s, %s, %s, %g]' % tuple(countsUnderLimit[i]))
            self.reporter.writeLine("\nthe " + str(lowToShow) + " biggest chroms with low counts:")
            countsUnderLimit.sort(key=lambda size: int(size[1]), reverse=True)
            for i in range(lowToShow):
                self.reporter.writeLine('[%s, %s, %s, %g]' % tuple(countsUnderLimit[i]))

    def __writeOverLimitInfo(self, countsOverLimit, maxToShow):
        """Writes info for chroms with highest counts."""
        countsOverTotal = len(countsOverLimit)
        highToShow = min(countsOverTotal, 5)
        self.reporter.writeLine("\ntotal chroms above upper limit: " + str(countsOverTotal))
        if countsOverTotal > 0:
            self.reporter.writeLine("\nthe " + str(highToShow) + " with highest counts per size:")
            countsOverLimit.sort(key=lambda countPer: float(countPer[3]), reverse=True)
            for i in range(highToShow):
                self.reporter.writeLine('[%s, %s, %s, %g]' % tuple(countsOverLimit[i]))
            self.reporter.writeLine("\nthe " + str(highToShow) + " biggest chroms with high counts:")
            countsOverLimit.sort(key=lambda size: int(size[1]), reverse=True)
            for i in range(highToShow):
                self.reporter.writeLine('[%s, %s, %s, %g]' % tuple(countsOverLimit[i]))
 
    def __countPerChrom(self):
        """Finds largest chromosomes with a count of zero, and finds chroms that have item count
        per megabase >3 standard deviations above or <1 SD below the mean. Finds largest of these
        chroms. Records output in reporter."""
        self.reporter.beginStep(self.db, self.table, "\ncountPerChrom stats:\n")
        self.reporter.writeStepInfo()
        counts = self.__getChromCountsFromDatabase()
        mean, SD = self.__getMeanAndSD(counts)
        upperLimit = mean + 3*SD
        lowerLimit = mean - SD
        self.reporter.writeLine("total chroms: " + str(len(counts)))
        self.reporter.writeLine("mean count/megabase = " + '%g' % mean)
        self.reporter.writeLine("standard deviation = " + '%g' % SD)
        self.reporter.writeLine("upper limit (mean + 3*SD) = " + '%g' % upperLimit)
        self.reporter.writeLine("lower limit (mean - SD) = " + '%g' % lowerLimit) 
        self.reporter.writeLine("lists below show: " +\
                                str(['chrom', 'chrom size', 'count', 'count/megabase']))
        countsOverLimit = []
        countsUnderLimit = []
        for row in counts:
            if row[3] > upperLimit:
                countsOverLimit.append(row)
            if row[3] < lowerLimit:
                countsUnderLimit.append(row)
        zeros = [row for row in counts if row[2] == '0']
        maxToShow = 5
        self.__writeZerosInfo(zeros, maxToShow)
        self.__writeUnderLimitInfo(countsUnderLimit, maxToShow)
        self.__writeOverLimitInfo(countsOverLimit, maxToShow)
        self.reporter.endStep()

    def __featureBits(self):
        """Adds featureBits output (both regular and overlap w/ gap) to sumRow.
        Also records commands and results in reporter"""
        fbCommand = ["featureBits", "-countGaps", self.db, self.table]
        fbOut, fbErr = qaUtils.runCommand(fbCommand)
        # normal featureBits output actually goes to stderr
        self.reporter.writeLine(' '.join(fbCommand))
        self.reporter.writeLine(str(fbErr))
        self.sumRow.setFeatureBits(fbErr.rstrip("in intersection\n"))
        fbGapCommand = ["featureBits", "-countGaps", self.db, self.table, "gap"]
        fbGapOut, fbGapErr = qaUtils.runCommand(fbGapCommand)
        self.reporter.writeLine(' '.join(fbGapCommand))
        self.reporter.writeLine(str(fbGapErr))
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
        self.__countPerChrom()
