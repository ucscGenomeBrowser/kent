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

    def __featureBits(self):
        """Runs featureBits -countGaps for this table and this table intersected with gap."""
        pass

    def validate(self):
        """Adds positional-table-specific checks to basic table checks."""
        super(PositionalQa, self).validate()

    def statistics(self):
        pass
