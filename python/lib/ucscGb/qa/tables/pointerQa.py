from ucscGb.qa.tables.tableQa import TableQa

class PointerQa(TableQa):
    """
    A pointer type of table, for tables consisting of one row that is a pointer to a file.
    """

    def validate(self):
        """Adds pointer-specific table checks to errorLog."""
        super(PointerQa, self).validate()
