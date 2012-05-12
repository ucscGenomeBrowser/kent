from ucscGb.qa.tables.tableQa import TableQa
from ucscGb.qa.tables.positionalQa import PositionalQa

class PointerQa(PositionalQa):
    """
    A pointer type of table, for tables consisting of one row that is a pointer to a file.
    These are not themselves positional tables, but we do need to do at least one check (for
    label lengths) that positional tables get.
    """

    def validate(self):
        """Adds pointer-specific table checks to errorLog."""
        TableQa.validate(self)
        PositionalQa.checkLabelLengths(self)

    def statistics(self):
        TableQa.statistics(self)
