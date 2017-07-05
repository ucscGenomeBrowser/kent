import pipes
from ucscGb.qa import qaUtils

def getTrackType(db, track):
    """Looks for a track type via tdbQuery."""
    cmd = ["tdbQuery", "select type from " + db + " where track='" + track +
           "' or table='" + track + "'"]
    cmdout, cmderr, cmdreturncode = qaUtils.runCommandNoAbort(cmd)
    
    if cmdreturncode != 0:
        # keep command arguments nicely quoted
        cmdstr = " ".join([pipes.quote(arg) for arg in cmd])
        raise Exception("Error from: " + cmdstr + ": " + cmderr)
    if cmdout:
        trackType = cmdout.split()[1]
    else:
        trackType = None
    return trackType

pslTypes = frozenset(["psl"])
genePredTypes = frozenset(["genePred"])
otherPositionalTypes = frozenset(["axt", "bed", "chain", "clonePos", "ctgPos", "expRatio", "maf",
                                  "netAlign", "rmsk", "sample", "wigMaf", "wig", "bedGraph",
                                  "chromGraph", "factorSource", "bedDetail", "pgSnp", "altGraphX",
                                  "ld2", "bed5FloatScore", "bedRnaElements", "broadPeak", "gvf",
                                  "narrowPeak", "peptideMapping"])
pointerTypes = frozenset(["bigWig", "bigBed", "bigPsl", "bigGenePred", "bigMaf", "bigChain",
                          "halSnake", "bam", "vcfTabix"])

# Instead of directly interfacing with sets above, can just use access functions
def isPsl(trackType):
    """ Access function for pslTypes set"""
    if trackType in pslTypes:
        return True

def isGenePred(trackType):
    """ Access function for genePredTypes set"""
    if trackType in genePredTypes:
        return True

def isPositional(trackType):
    """ Access function for otherPositonalTypes set"""
    if trackType in otherPositionalTypes:
        return True

def isPointer(trackType):
    """ Access function for pointerTypes set"""
    if trackType in pointerTypes:
        return True

# group the types of positional tracks by the name of the column that contains the chrom
# couldn't find any existing chromGraph tracks, so left it out for now
chromTypes = frozenset(['bed', 'genePred', 'axt', 'clonePos', 'ctgPos', 'expRatio', 'maf', 'sample',
                        'wigMaf', 'wig', 'bedGraph', 'factorSource', 'bedDetail', 'Ld2',
                        'bed5FloatScore', 'bedRnaElements', 'broadPeak', 'gvf', 'narrowPeak',
                        'peptideMapping', 'pgSnp'])
tNameTypes = frozenset(['chain', 'psl', 'altGraphX', 'netAlign'])
genoNameTypes = frozenset(['rmsk'])

def getChromCol(trackType):
    """Returns the name of the column that contains the chrom name, based on track type."""
    if trackType in chromTypes:
        return 'chrom'
    elif trackType in genoNameTypes:
        return 'genoName'
    elif trackType in tNameTypes:
        return 'tName'
    else:
        raise Exception("Can't find column name for track type " + trackType)

def getChrStartEndCols(trackType):
    """Get columns names for chromosome, chromosome start and chromosome end
       based on track type"""
    # genePred chromsome start/end col names are slightly different
    # than others in chromType list
    if trackType == "genePred":
        colNames = ("chrom","txStart","txEnd")
    elif trackType in chromTypes:
        colNames = ("chrom", "chromStart", "chromEnd")
    elif trackType in genoNameTypes:
        colNames = ("genoName", "genoStart", "genoEnd")
    elif trackType in tNameTypes:
        colNames = ("tName", "tStart", "tEnd")
    else:
        raise Exception("Can't find chrom, start, end column names for track type " + trackType)
    return colNames

def checkCanPack(db, table):
    """Check if a track can be set to pack"""
    visibility = "pack"
    canPackOut = qaUtils.callHgsql(db, "select canPack from trackDb where tableName='" + table + "'")

    if canPackOut != 1:
        visibility = "full"

    return visibility

def getAttributeForTrack(attribute, db, track):
    """Uses tdbQuery to get an attribute where track or table == 'track' and removes label."""
    cmd = ["tdbQuery", "select " + attribute + " from " + db + " where table='" + track +
           "' or track='" + track + "'"]
    cmdout, cmderr = qaUtils.runCommand(cmd)
    return cmdout.strip(attribute).strip()

def getParent(db, track):
    """Returns name of parent.  Looks for either 'parent' or 'subtrack' setting in trackDb."""
    parent = getAttributeForTrack("parent", db, track)
    if not parent:
        parent = getAttributeForTrack("subTrack", db, track)
    if parent:
        parent = parent.split()[0] # get rid of the "on" or "off" setting if present
    return parent

def getLabels(db, track, labelType):
    """Returns labels of specified 'type' (shortLabel or longLabel) for a single track,
       its parent, and its superTrack. Excludes 'view' labels."""

    labels = []
    # need to add the first labels regardless of 'view' setting, which is inherited
    labels.append(getAttributeForTrack(labelType, db, track))

    parent = getParent(db, track)
    view = getAttributeForTrack("view", db, track)
    if view:
        viewParent = getParent(db, parent)
        labels.append(getAttributeForTrack(labelType, db, viewParent))
    elif parent:
        labels.append(getAttributeForTrack(labelType, db, parent))


    superTrack = getAttributeForTrack("superTrack", db, track)
    if superTrack:
        superTrackSplit = superTrack.strip().split(" ")
        superTrackName = superTrackSplit[0]
        labels.append(getAttributeForTrack(labelType, db, superTrackName))
    return labels

def makeBedFromTable(db, table, whereOpt=""):
    """Make BED3 out of a table"""
    # Get chrom, start, and end column names
    trackType = getTrackType(db, table)
    columnNames = getChrStartEndCols(trackType)

    tableList = checkSplit(db, table)

    # whereOpt allows you to select only certain items from table, e.g. only unbridged gaps
    if whereOpt:
        whereOpt = " where " + whereOpt
    output = ""
    for tbl in tableList:
            hgsqlCmd = "select " + columnNames[0] + ", " + columnNames[1] + ", " \
                        + columnNames[2] + " from " + tbl + whereOpt
            hgsqlOut = qaUtils.callHgsql(db, hgsqlCmd)
            output += hgsqlOut
    return output

def checkSplit(db, table):
    """Check if table is split by chromosome"""
    splitCheck = qaUtils.callHgsql(db, "show tables like '" + table + "'")
    splitCheck = splitCheck.strip()

    splitTableList = []
    if splitCheck == table:
        splitTableList.append(table)
    elif splitCheck == "":
        hgsqlTblsOut = qaUtils.callHgsql(db, "show tables like 'chr%" + table + "%'")
        splitTableList = hgsqlTblsOut.split('\n')
        splitTableList.remove("")
    return splitTableList
