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
        tdbOut, tdbErr = qaUtils.runCommand(tdbCommand)
        # Turn tdbOut into array so we can extract different pieces
        tdbOut = tdbOut.replace("\n","")
        tdbOut = tdbOut.split(" ")
        # tdbOut[1] should be what option is used for baseColorUseSequence
        # We want to fully run pslCheck for those entries with "table" and "extFile"
        if len(tdbOut) > 2:
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
            # Check if $db.sizes file exists
            if not os.path.isfile("%s.chrom.sizes" % self.db):
                # If it doens't exist, get chrom sizes from chromInfo
                chromSizesOut = qaUtils.callHgsql(self.db, "select * from chromInfo")
                chromSizes = open("%s.chrom.sizes" % self.db, 'w')
                chromSizes.write(chromSizesOut)
                chromSizes.close()
            # Run more in-depth version of pslCheck
            command = ("pslCheck", "-querySizes=%s.sizes" % self.table ,\
                    "-targetSizes=%s.chrom.sizes" % self.db, "-db=" + self.db, self.table)
            self.reporter.writeCommand(command)
            commandOut, commandErr, commandReturnCode = qaUtils.runCommandNoAbort(command)
            # Write output to file
           self.reporter.fh.write(commandErr)
            # Clean up intermediate item sizes file
            os.remove("%s.sizes" % self.table)

        # For everything else, use generic set of steps
        else:
            command = ["pslCheck", "db=" + self.db, self.table]
            self.reporter.writeCommand(command)
            commandOut, commandErr, commandReturnCode = qaUtils.runCommandNoAbort(command)
           self.reporter.fh.write(commandErr)

        if commandReturnCode:
            self.recordError()
        else:
            self.recordPass()
        self.reporter.endStep()

    def validate(self):
        """Adds psl-specific table checks to basic table checks."""
        super(PslQa, self).validate()
        self.__pslCheck()
