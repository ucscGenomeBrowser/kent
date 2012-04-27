import re

from ucscgenomics.qa import qaUtils
from ucscgenomics.qa.tables import summary

class TableQa(object):
    """
    A generic database table.  Base class for other types of track table types (psl, genePred,
    etc.), for running QA validations and describing table statistics.
    """

    def __init__(self, db, table, tableType, reporter, sumTable):
        self.db = db
        self.table = table
        self.reporter = reporter
        self.sumTable = sumTable
        self.sumRow = summary.SumRow(db, table, tableType)
        self.sumTable.addRow(self.sumRow)

    def __checkTableDescription(self):
        """Checks for an autoSql definition for this table in the tableDescriptions table."""
        # Ideally this would also check to see if each column in the table is described.
        self.reporter.beginStep(self.db, self.table, "checking table descriptions")
        self.reporter.writeStepInfo()
        sqlOut = qaUtils.callHgsql(self.db, "select autoSqlDef from tableDescriptions where\
                                  tableName='" + self.table + "'")
        if sqlOut.strip() == '':
            self.reporter.writeLine("ERROR: No table description for " + self.db + "." + self.table)
            self.sumRow.setError()
        else:
            self.reporter.writeLine("pass")
        self.reporter.endStep()

    def __checkForUnderscores(self):
        """Checks the table name for underscores. Allows 'all_' and 'chr.*_' for split tables."""
        self.reporter.beginStep(self.db, self.table, "checking for underscores")
        self.reporter.writeStepInfo()
        if re.search('.*_.*', self.table) and not re.search('^(chr.*|all)_.*', self.table):
            self.reporter.writeLine("ERROR: " + self.db + "." + self.table + 
                                    " has unexpected underscores")
            self.sumRow.setError()
        else:
            self.reporter.writeLine("pass")
        self.reporter.endStep()

    def __getRowCount(self):
        """Returns the number of rows in this table."""
        pass

    def validate(self):
        """Runs validation methods.  Puts errors captured from programs in errorLog."""
        self.__checkTableDescription()
        self.__checkForUnderscores()

    def statistics(self):
        """Returns a table stats object describing statistics of this table."""
        pass
