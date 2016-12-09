import subprocess
import os

from ucscGb.qa.tables.positionalQa import PositionalQa
from ucscGb.qa import qaUtils

class PslQa(PositionalQa):
    """
    A psl type of table.
    """

    def __pslCheck(self):
        """Runs pslCheck program on this table and sends result to reporter's filehandle."""
        self.reporter.beginStep(self.db, self.table, "pslCheck")
        # Get baseColorUseSequence setting
        tdbCommand = ["tdbQuery", "select baseColorUseSequence from " + self.db +\
                " where track='" + self.table +"' or table='"+ self.table +"'"]
        p = subprocess.Popen(tdbCommand, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        #extract stdout/stderr from Popen command
        tdbOut, tdbErr = p.communicate()
        #Turn tdbOut into array so we can extract different pieces
        tdbOut = tdbOut.replace("\n","")
        tdbOut = tdbOut.split(" ")
        # tdbOut[1] should be what option is used for baseColorUseSequence
        # We want to fully run pslCheck for those entries with "table" and "extFile"
        if tdbOut[1] == "table" or tdbOut[1] == "extFile":
            if tdbOut[1] == "table":
                # Get sizes of sequences in specified table
                sizesOut = qaUtils.callHgsql(self.db, "select name, length(seq) from " + tdbOut[2])
            elif tdbOut[1] == "extFile":
                # Extract sizes from named seq table
                sizesOut = qaUtils.callHgsql(self.db, "select acc, size from " + tdbOut[2])

            # Write itemSizes to file
            itemSizes = open("%s.sizes" % self.table, 'w')
            itemSizes.write(sizesOut)
            itemSizes.close()
            # get chrom sizes from chromInfo
            chromSizesOut = qaUtils.callHgsql(self.db, "select * from chromInfo")
            chromSizes = open("%s.chrom.sizes" % self.db, 'w')
            chromSizes.write(chromSizesOut)
            chromSizes.close()
            # Run more in-depth version of pslCheck
            command = ("pslCheck", "-querySizes=%s.sizes" % self.table ,\
                    "-targetSizes=%s.chrom.sizes" % self.db, "db=" + self.db, self.table)
            self.reporter.writeCommand(command)
            p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
            pslCheckOut, pslCheckErr = p.communicate()
            #Clean up intermediate files
            os.remove("%s.sizes" % self.table)
            os.remove("%s.chrom.sizes" % self.db)

        # For everything else, use generic set of steps
        else:
            command = ["pslCheck", "db=" + self.db, self.table]
            self.reporter.writeCommand(command)
            p = subprocess.Popen(command, stdout=self.reporter.fh, stderr=self.reporter.fh)
            pslCheckOut, pslCheckErr = p.communicate()

        p.wait()
        if p.returncode:
            self.recordError()
        else:
            self.recordPass()
        self.reporter.endStep()

    def validate(self):
        """Adds psl-specific table checks to basic table checks."""
        super(PslQa, self).validate()
        self.__pslCheck()
