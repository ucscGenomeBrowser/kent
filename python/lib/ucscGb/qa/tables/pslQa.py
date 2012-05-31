import subprocess

from ucscGb.qa.tables.positionalQa import PositionalQa
from ucscGb.qa import qaUtils

class PslQa(PositionalQa):
    """
    A psl type of table.
    """

    def __pslCheck(self):
        """Runs pslCheck program on this table and sends result to reporter's filehandle."""
        self.reporter.beginStep(self.db, self.table, "pslCheck")
        command = ["pslCheck", "db=" + self.db, self.table]
        self.reporter.writeCommand(command)
        p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
        p.wait()
        if p.returncode:
            self.recordError()
        else:
            self.recordPass()
        self.reporter.endStep()

    def validate(self):
        """Adds psl-specific table checks to errorLog."""
        super(PslQa, self).validate()
        self.__pslCheck()
