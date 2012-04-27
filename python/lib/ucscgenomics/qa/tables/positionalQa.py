import subprocess

from ucscgenomics.qa import qaUtils
from ucscgenomics.qa.tables.tableQa import TableQa

genbankTableListDev = "/cluster/data/genbank/etc/genbank.tbls"
genbankTableListBeta = "/genbank/etc/genbank.tbls"

class PositionalQa(TableQa):
    """
    A positional table.
    """

    def __checkLabelLengths(self):
        pass

    def __writePassOrFail(self, returncode):
        if returncode == 0:
            self.reporter.writeLine("pass")
        else:
            self.reporter.writeLine("ERROR")
            self.sumRow.setError()

    def __positionalTblCheck(self):
        """Runs positionalTblCheck program on this table. Excludes GenBank tables."""
        #TODO: decide how to exclude genbank tables
        self.reporter.beginStep(self.db, self.table, "positionalTblCheck")
        command = ["positionalTblCheck", self.db, self.table]
        self.reporter.writeCommand(command)
        p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
        p.wait()
        self.__writePassOrFail(p.returncode)
        self.reporter.endStep()

    def __checkTableCoords(self):
        """Runs checkTableCoords program on this table."""
        self.reporter.beginStep(self.db, self.table, "checkTableCoords")
        command = ["checkTableCoords", self.db, self.table]
        self.reporter.writeCommand(command)
        p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
        p.wait()
        self.__writePassOrFail(p.returncode)
        self.reporter.endStep()

    def __chromosomeCoverage(self):
        """Returns a chrom counts object with the number of items per chromosome in this table."""
        pass

    def __featureBits(self):
        """Runs featureBits -countGaps for this table and this table intersected with gap."""
        pass

    def validate(self):
        """Adds positional-table-specific checks to basic table checks."""
        super(PositionalQa, self).validate()
        self.__checkLabelLengths()
        self.__positionalTblCheck()
        self.__checkTableCoords()

    def statistics(self):
        pass
