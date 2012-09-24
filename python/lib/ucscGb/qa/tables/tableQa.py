import re

from ucscGb.qa import qaUtils
from ucscGb.qa.tables import summary

class TableQa(object):
    """
    A generic database table.  Base class for other types of track table types (psl, genePred,
    etc.), for running QA validations and describing table statistics. Requires a Reporter for
    logging error output and a SumTable for keeping track of statistics and error status.
    """

    def __init__(self, db, table, tableType, reporter, sumTable):
        self.db = db
        self.table = table
        self.tableType = tableType
        self.reporter = reporter
        self.sumTable = sumTable
        self.sumRow = summary.SumRow(db, table, tableType)
        self.sumTable.addRow(self.sumRow)

    def recordPass(self):
        """Writes the word pass to reporter. 'Public' so that TableQa subclasses can use it."""
        self.reporter.writeLine("pass")

    def recordError(self):
        """Writes the word ERROR to reporter, and sets error status in sumRow. 'Public' so that
        TableQa subclasses can use it."""
        self.reporter.writeLine("ERROR")
        self.sumRow.setError()

    def __checkTableDescription(self):
        """Checks for an autoSql definition for this table in the tableDescriptions table."""
        # Ideally this would also check to see if each column in the table is described.
        self.reporter.beginStep(self.db, self.table, "checking for table descriptions")
        self.reporter.writeStepInfo()
        sqlOut = qaUtils.callHgsql(self.db, "select autoSqlDef from tableDescriptions where\
                                  tableName='" + self.table + "'")
        if sqlOut.strip() == '':
            self.reporter.writeLine("No table description for " + self.db + "." + self.table)
            self.recordError()
        else:
            self.recordPass()
        self.reporter.endStep()

    def __underscoreAllowed(self, table):
        """Returns match if table name has one of the excepted underscores."""
        return re.search('^(chr.*|all|trackDb|hgFindSpec)_.*', table)

    def checkForUnderscores(self):
        """Checks the table name for underscores. Knows about exceptions. Method is "public" for
        use by other table types."""
        self.reporter.beginStep(self.db, self.table, "checking table name for underscores")
        self.reporter.writeStepInfo()
        if re.search('.*_.*', self.table) and not self.__underscoreAllowed(self.table):
            self.reporter.writeLine(self.db + "." + self.table + " has unexpected underscores")
            self.recordError()
        else:
            self.recordPass()
        self.reporter.endStep()

    def __rowCount(self):
        """Adds the number of rows in this table to the sumRow."""
        rowCount = qaUtils.callHgsql(self.db, "select count(*) from " + self.table)
        self.sumRow.setCount(rowCount.strip())

    def validate(self):
        """Runs validation methods. Sends errors captured from programs to reporter."""
        self.__checkTableDescription()
        self.checkForUnderscores()

    def statistics(self):
        """Adds statistics to sumRow."""
        self.__rowCount()
