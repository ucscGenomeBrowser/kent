class TableQa(object):
"""
A generic database table.  Base class for other types of track table types (psl, genePred, etc.),
for running QA validations and describing table statistics.
"""

    def __init__(self, db, table):
        self.db = db
        self.table = table

    def __checkDescriptions(self):
        """Checks for an autoSql definition for this table in the tableDescriptions table."""
        # Ideally this would also check to see if each column in the table is described.
        pass

    def __checkNameFormat(self):
        """Checks the table name for underscores."""
        pass

    def __getRowCount(self):
        """Returns the number of rows in this table."""
        pass

    def validate(self, errorLog):
        """Runs validation methods.  Puts errors captured from programs in errorLog."""
        pass

    def statistics(self):
    """Returns a table stats object describing statistics of this table."""
        pass
