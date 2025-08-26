from ucscGb.qa.tables.positionalQa import PositionalQa
from ucscGb.qa import qaUtils

class GenePredQa(PositionalQa):
    """
    A genePred type of table.
    """
    def __genePredCheck(self):
        """Runs genePredCheck program on this table and sends results to reporter's filehandle."""
        self.reporter.beginStep(self.db, self.table, "genePredCheck")
        command = ["genePredCheck", "db=" + self.db, self.table]
        self.reporter.writeCommand(command)
        commandOut, commandErr, commandReturnCode = qaUtils.runCommandNoAbort(command)
        self.reporter.fh.write(commandErr)
        if commandReturnCode:
            self.recordError()
        else:
            self.recordPass()
        self.reporter.endStep()

    def validate(self):
        """Adds genePred-specific table checks to basic table checks."""
        super(GenePredQa, self).validate()
        self.__genePredCheck()
