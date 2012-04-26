from ucscgenomics.qa.tables.positionalQa import PositionalQa
from ucscgenomics.qa import qaUtils

class GenePredQa(PositionalQa):
    """
    A genePred type of table.
    """
    def __genePredCheck(self):
        """Runs genePredCheck program on this table and sends results to reporter's filehandle."""
        self.reporter.beginStep(self.db, self.table, "genePredCheck")
        self.reporter.writeStepInfo()
        command = ["genePredCheck", "db=" + self.db, self.table]
        self.reporter.writeCommand(command)
        qaUtils.runCommand(command, self.reporter.fh, self.reporter.fh)
        self.reporter.endStep()
        self.reporter.writeBlankLine()

    def validate(self):
        """Adds genePred-specific table checks to errorLog."""
        super(GenePredQa, self).validate()
        self.__genePredCheck()
