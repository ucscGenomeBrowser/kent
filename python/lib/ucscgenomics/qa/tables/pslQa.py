from ucscgenomics.qa.tables.positionalQa import PositionalQa
from ucscgenomics.qa import qaUtils

class PslQa(PositionalQa):
    """
    A psl type of table.
    """

    def __pslCheck(self):
        """Runs pslCheck program on this table and sends result to reporter's filehandle."""
        self.reporter.beginStep(self.db, self.table, "pslCheck")
        self.reporter.writeStepInfo()
        command = ["pslCheck", "db=" + self.db, self.table]
        self.reporter.writeCommand(command)
        qaUtils.runCommand(command, self.reporter.fh, self.reporter.fh)
        self.reporter.endStep()
        self.reporter.writeBlankLine()

    def validate(self):
        """Adds psl-specific table checks to errorLog."""
        super(PslQa, self).validate()
        self.__pslCheck()
