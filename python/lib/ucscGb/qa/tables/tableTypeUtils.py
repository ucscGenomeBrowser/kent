import pipes
from ucscGb.qa import qaUtils

def getTrackType(db, table):
    """Looks for a track type via tdbQuery."""
    cmd = ["tdbQuery", "select type from " + db + " where track='" + table +
           "' or table='" + table + "'"]
    cmdout, cmderr, cmdreturncode = qaUtils.runCommandNoAbort(cmd)
    
    if cmdreturncode != 0:
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
                                  "chromGraph", "factorSource", "bedDetail", "pgSnp", "altGraphX",
                                  "ld2", "bed5FloatScore", "bedRnaElements", "broadPeak", "gvf",
                                  "narrowPeak", "peptideMapping"])
pointerTypes = frozenset(["bigWig", "bigBed", "bigPsl", "bigGenePred", "bigMaf", "bigChain",
                          "halSnake", "bam", "vcfTabix"])

# Instead of directly interfacing with sets above, can just just use access functions
def isPsl(tableType):
    """ Access function for pslTypes set"""
    if tableType in pslTypes:
        return True

def isGenePred(tableType):
    """ Access function for genePredTypes set"""
    if tableType in genePredTypes:
        return True

def isPositional(tableType):
    """ Access function for otherPositonalTypes set"""
    if tableType in otherPositionalTypes:
        return True

def isPointer(tableType):
    """ Access function for pointerTypes set"""
    if tableType in pointerTypes:
        return True

# group the types of positional tables by the name of the column that contains the chrom
# couldn't find any existing chromGraph tables, so left it out for now
chromTypes = frozenset(['bed', 'genePred', 'axt', 'clonePos', 'ctgPos', 'expRatio', 'maf', 'sample',
                        'wigMaf', 'wig', 'bedGraph', 'factorSource', 'bedDetail', 'Ld2',
                        'bed5FloatScore', 'bedRnaElements', 'broadPeak', 'gvf', 'narrowPeak',
                        'peptideMapping', 'pgSnp'])
tNameTypes = frozenset(['chain', 'psl', 'altGraphX', 'netAlign'])
genoNameTypes = frozenset(['rmsk'])

def getChromCol(tableType):
    """Returns the name of the column that contains the chrom name, based on track type."""
    if tableType in chromTypes:
        return 'chrom'
    elif tableType in genoNameTypes:
        return 'genoName'
    elif tableType in tNameTypes:
        return 'tName'
    else:
        raise Exception("Can't find column name for track type " + tableType)

def getChrStartEndCols(tableType):
    """Get columns names for chromosome, chromosome start and chromosome end
       based on table type"""
    # genePred chromsome start/end col names are slightly different
    # that others in chromType list
    if tableType == "genePred":
        colNames = ("chrom","txStart","txEnd")
    elif tableType in chromTypes:
        colNames = ("chrom", "chromStart", "chromEnd")
    elif tableType in genoNameTypes:
        colNames = ("genoName", "genoStart", "genoEnd")
    elif tableType in tNameTypes:
        colNames = ("tName", "tStart", "tEnd")
    else:
        raise Exception("Can't find chrom, start, end column names for track type " + tableType)

    return colNames

