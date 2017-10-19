import re
import math

from ucscGb.qa import qaUtils
from ucscGb.qa.tables import trackUtils
from ucscGb.qa.tables import checkGapOverlap
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

    def checkLabelLengths(self):
        """Checks that short and long labels (for this track + parents) are shorter than limits.
        Writes labels to reporter. Ignores 'view' labels. Makes an exception for shortLabel 
        '$o_Organism Chain/Net'."""
        self.reporter.beginStep(self.db, self.table, "checking label lengths:")
        self.reporter.writeStepInfo()
        shortLabels = trackUtils.getLabels(self.db, self.table, "shortLabel")
        longLabels = trackUtils.getLabels(self.db, self.table, "longLabel")

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
            commandOut, commandErr, commandReturnCode = qaUtils.runCommandNoAbort(command)
            self.reporter.fh.write(commandErr)
            if commandReturnCode:
                self.recordError()
            else:
                self.recordPass()
            self.reporter.endStep()

    def __checkTableCoords(self):
        """Runs checkTableCoords program on this table."""
        self.reporter.beginStep(self.db, self.table, "checkTableCoords")
        command = ["checkTableCoords", self.db, self.table]
        self.reporter.writeCommand(command)
        commandOut, commandErr, commandReturnCode = qaUtils.runCommandNoAbort(command)
        self.reporter.fh.write(commandErr)
        if commandReturnCode:
            self.recordError()
        else:
            self.recordPass()
        self.reporter.endStep()

    def __getChromCountsFromDatabase(self):
        """Returns a list of ['chrom', 'size (bases)', 'count', 'count/megabase'] lists for table"""
        chromCol = trackUtils.getChromCol(self.tableType)
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
        """Runs featureBits. Adds output to sumRow and records commands and results in reporter."""

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

        # Check overlap with gaps, both bridged and unbridged
        gapOverlapOut = ''
        if self.table != 'gap':
            gapOverlapOut = checkGapOverlap.checkGapOverlap(self.db, self.table, True)
        self.reporter.writeLine(gapOverlapOut)

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
