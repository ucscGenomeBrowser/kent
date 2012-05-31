import subprocess

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
        p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
        p.wait()
        if p.returncode:
            self.recordError()
        else:
            self.recordPass()
        self.reporter.endStep()

    def validate(self):
        """Adds genePred-specific table checks to errorLog."""
        super(GenePredQa, self).validate()
        self.__genePredCheck()
