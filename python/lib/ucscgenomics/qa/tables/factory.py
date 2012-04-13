

def __getTrackType(db, table):
    """Looks for a track type via tdbQuery."""
    cmd = ["tdbQuery", "select type from " + database + " where track='" + table +
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
otherPositionalTypes = frozenset(["axt", "bed", "chain", "clonePos", "ctgPos", "expRatio",
                                  "maf", "netAlign", "rmsk", "sample", "wigMaf", "wig", "bedGraph",
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
        raise Exception(database + table + " has unknown track type " + tableType)

