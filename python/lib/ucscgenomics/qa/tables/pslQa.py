from ucscgenomics.qa.tables.positionalQa import PositionalQa

class PslQa(PositionalQa):
    """
    A psl type of table.
    """

    def __pslCheck(self):
        """Runs pslCheck program on this table. """
        pass

    def validate(self, errorLog):
        """Adds psl-specific table checks to errorLog."""
        pass
