import time
from pycbio.sys import procOps

class Reporter(object):

    """
    Reports consistently-formatted test results to a file.

    output database/table/time

    begin a step
    output database/table/time
    output command line
    output from step
    end step

    """

    def __init__(self, fh):
        self.fh = fh
        self.db = None
        self.table = None
        self.description = None

    @staticmethod
    def __formatCommand(command):
        """Turns command list into string, keeping whitespace quoted."""
        return " ".join(procOps.shQuote(command))

    def beginStep(self, db, table, description):
        """To be run at the beginning of each test. Sets variables."""
        assert self.db == None
        self.db = db
        self.table = table
        self.description = description

    def writeLine(self, line):
        """Writes line to file handle followed by newline."""
        self.fh.write(line + "\n")
        self.fh.flush()

    def writeBlankLine(self):
        """Writes an empty line to the file."""
        self.fh.write("\n")
        self.fh.flush()

    def writeCommand(self, command):
        """Formats command list as string and writes it to file. """
        self.fh.write(self.__formatCommand(command) + "\n")
        self.fh.flush()

    def writeTimestamp(self):
        """Writes the current date and time to the file."""
        self.fh.write(time.asctime() + "\n")
        self.fh.flush()

    def writeStepInfo(self):
        """Writes current step information and timestamp to file."""
        self.fh.write(self.db + " " + self.table + " " + self.description + "\n")
        self.fh.flush()

    def endStep(self):
        """To be run at the end of each test. Clears variables and writes a blank line."""
        self.db = None
        self.table = None
        self.description = None
        self.writeBlankLine()
