from ucscgenomics.qa import qaUtils

class TableQa(object):
    """
    A generic database table.  Base class for other types of track table types (psl, genePred,
    etc.), for running QA validations and describing table statistics.
    """

    def __init__(self, db, table, reporter):
        self.db = db
        self.table = table
        self.reporter = reporter

    def __tableDescriptionCheck(self):
        """Checks for an autoSql definition for this table in the tableDescriptions table."""
        # Ideally this would also check to see if each column in the table is described.
        self.reporter.beginStep(self.db, self.table, "table descriptions check")
        self.reporter.writeStepInfo()
        sqlOut = qaUtils.callHgsql(self.db, "select autoSqlDef from tableDescriptions where\
                                  tableName='" + self.table + "'")
        if sqlOut.strip() == '':
            self.reporter.writeLine("ERROR: No table description for " + self.db + "." + self.table)
        self.reporter.endStep()
        self.reporter.writeBlankLine()

    def __checkNameFormat(self):
        """Checks the table name for underscores."""
        pass

    def __getRowCount(self):
        """Returns the number of rows in this table."""
        pass

    def validate(self):
        """Runs validation methods.  Puts errors captured from programs in errorLog."""
        self.__tableDescriptionCheck()

    def statistics(self):
        """Returns a table stats object describing statistics of this table."""
        pass
