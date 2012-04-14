import subprocess
import pipes
from ucscgenomics.qa.tables.genePredQa import GenePredQa
from ucscgenomics.qa.tables.tableQa import TableQa
from ucscgenomics.qa.tables.pslQa import PslQa
from ucscgenomics.qa.tables.positionalQa import PositionalQa
from ucscgenomics.qa.tables.pointerQa import PointerQa

# TODO: add functions that check whether db and table actually exists

def getTrackType(db, table):
    """Looks for a track type via tdbQuery."""
    cmd = ["tdbQuery", "select type from " + db + " where track='" + table +
           "' or table='" + table + "'"]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    cmdout, cmderr = p.communicate()
    if p.returncode != 0:
        # keep command arguments nicely quoted
        cmdstr = " ".join([pipes.quote(arg) for arg in cmd])
        raise Exception("Error from: " + cmdstr + ": " + cmderr)
    if cmdout:
        tableType = cmdout.split()[1]
    else:
        tableType = None
    return tableType

pslTypes = frozenset(["psl"])
genePredTypes = frozenset(["genePred"])
otherPositionalTypes = frozenset(["axt", "bed", "chain", "clonePos", "ctgPos", "expRatio", "maf",
                                  "netAlign", "rmsk", "sample", "wigMaf", "wig", "bedGraph",
                                  "chromGraph", "factorSource", "bedDetail", "pgSnp"])
pointerTypes = frozenset(["bigWig", "bigBed", "bam"])

def tableQaFactory(db, table):
    """Returns tableQa object according to trackDb track type.""" 
    tableType = getTrackType(db, table)
    if not tableType:
        return TableQa(db, table)
    elif tableType in pslTypes:
        return PslQa(db, table)
    elif tableType in genePredTypes:
        return GenePredQa(db, table)
    elif tableType in otherPositionalTypes:
        return PositionalQa(db, table)
    elif tableType in pointerTypes:
        return PointerQa(db, table)
    else:
        raise Exception(db + table + " has unknown track type " + tableType)

