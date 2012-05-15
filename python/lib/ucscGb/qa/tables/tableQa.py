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
        self.reporter = reporter
        self.sumTable = sumTable
        self.sumRow = summary.SumRow(db, table, tableType)
        self.sumTable.addRow(self.sumRow)

    def recordPassOrError(self, error):
        """Writes the word pass or ERROR to reporter, and sets error status in sumRow.
        Not really for TableQa public use (it is used by TableQa subclasses). """
        if not error:
            self.reporter.writeLine("pass")
        else:
            self.reporter.writeLine("ERROR")
            self.sumRow.setError()

    def __checkTableDescription(self):
        """Checks for an autoSql definition for this table in the tableDescriptions table."""
        # Ideally this would also check to see if each column in the table is described.
        self.reporter.beginStep(self.db, self.table, "checking for table descriptions")
        self.reporter.writeStepInfo()
        error = False
        sqlOut = qaUtils.callHgsql(self.db, "select autoSqlDef from tableDescriptions where\
                                  tableName='" + self.table + "'")
        if sqlOut.strip() == '':
            self.reporter.writeLine("ERROR: No table description for " + self.db + "." + self.table)
            error = True
        self.recordPassOrError(error)
        self.reporter.endStep()

    def checkForUnderscores(self):
        """Checks the table name for underscores. Allows 'all_' and 'chr.*_' for split tables, and
        'trackDb_*' and 'hgFindSpec_*'. Method is "public" for use by other table types."""
        self.reporter.beginStep(self.db, self.table, "checking table name for underscores")
        self.reporter.writeStepInfo()
        error = False
        if re.search('.*_.*', self.table) and not re.search('^(chr.*|all|trackDb|hgFindSpec)_.*',
                                                            self.table):
            self.reporter.writeLine("ERROR: " + self.db + "." + self.table + 
                                    " has unexpected underscores")
            error = True
        self.recordPassOrError(error)
        self.reporter.endStep()

    def __rowCount(self):
        """Adds the number of rows in this table to the sumRow."""
        rowCount = qaUtils.callHgsql(self.db, "select count(*) from " + self.table)
        self.sumRow.setCount(rowCount.strip())

    def validate(self):
        """Runs validation methods.  Puts errors captured from programs in errorLog."""
        self.__checkTableDescription()
        self.checkForUnderscores()

    def statistics(self):
        """Adds statistics to sumRow."""
        self.__rowCount()
