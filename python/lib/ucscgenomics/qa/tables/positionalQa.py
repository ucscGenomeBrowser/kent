from ucscgenomics.qa.tables.tableQa import TableQa

class PositionalQa(TableQa):
    """
    A positional table.
    """

    def __positionalTblCheck(self):
        """Runs positionalTblCheck program on this table. """
        pass

    def __checkTableCoords(self):
        """Runs checkTableCoords program on this table."""
        pass

    def __chromosomeCoverage(self):
        """Returns a chrom counts object with the number of items per chromosome in this table."""
        pass

    def validate(self, errorLog):
        """Adds positional-table-specific checks to basic table checks."""
        pass

