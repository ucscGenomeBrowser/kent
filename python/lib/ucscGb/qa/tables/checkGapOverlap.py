import tempfile
import stat
import os
        
from ucscGb.qa import qaUtils
from ucscGb.qa.tables import tableTypeUtils

def checkSplit(db, table):
    """Check if table is split"""
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

def checkCanPack(db, table):
    """Check if a table can be set to pack"""
    visibility = "pack"
    canPackOut = qaUtils.callHgsql(db, "select canPack from trackDb where tableName='" + table + "'") 

    if canPackOut != 1:
        visibility = "full"
    
    return visibility    

def constructOutputUrls(db, table, overlapFile):
    """ Using file of overlaping gaps, build URLs to genome-test
        centered around the first three overlaps"""
    outputUrls = ""
    lineCount = 0 # Used to keep running tally of number of lines in file
    i = 0 # Used to count number of URLs constructed so far
    if os.stat(overlapFile).st_size != 0:
        gapOverUrls = ""

        splitFile = overlapFile.split(".")
	if splitFile[2] == "unbr":
            outputUrls += "\nTotal number of overlaps with unbridged gaps:\n"
        else:
            outputUrls += "\nThere are gaps overlapping " + table + " (gaps may be bridged or not):\n"

        vis = checkCanPack(db, table)

	openOverlapFile = open(overlapFile)
        for line in openOverlapFile:
            lineCount += 1
            if i < 3:
                splitLine = line.strip().split("\t")
                chrom = splitLine[0]
                start = splitLine[1]
                end = splitLine[2]
                position = chrom + ":" + str(int(start) - 300) + "-" + str(int(end) + 300)
                color = "&highlight=" + db + "." + chrom + ":" + start + "-" + end + "#aaedff"
                url="http://genome-test.cse.ucsc.edu/cgi-bin/hgTracks?db=" + db + "&"\
                     + table + "=" + vis + "&gap=pack&hideTracks=1&position=" + position\
                     + color + "\n"
                gapOverUrls += url  
                i+=1  
        outputUrls += str(lineCount) + " " + overlapFile + "\n\n" + gapOverUrls
        openOverlapFile.close()
    return outputUrls

def makeBedFromTable(db, table):
    """Make BED3 tempfile out of a table"""
    # Get chrom, start, and end column names
    trackType = tableTypeUtils.getTrackType(db, table)
    columnNames = tableTypeUtils.getChrStartEndCols(trackType)

    tableList = checkSplit(db, table)
    tableBedTemp = tempfile.NamedTemporaryFile(mode='w')
    tableBed = open(tableBedTemp.name, 'w')
    for tbl in tableList:
            hgsqlCmd = "select " + columnNames[0] + ", " + columnNames[1] + ", " \
                        + columnNames[2] + " from " + tbl
            hgsqlOut = qaUtils.callHgsql(db, hgsqlCmd)
            tableBed.write(hgsqlOut)
    tableBed.close()

    return tableBedTemp

def checkGapOverlap(db, table, checkUnbridged=False):
    """Check if any items in a table over lap with bridged and unbridged gaps"""
    tableBedTemp = makeBedFromTable(db, table)

    gapOverlapOut = ""
    gapTableList = checkSplit(db, "gap")
    gapFileName = str(db) + ".gap.bed.temp"
    gapFile = open(gapFileName, 'w')
    for tbl in gapTableList:
            hgsqlGapOut = qaUtils.callHgsql(db, "select chrom, chromStart, chromEnd from " + tbl)
            gapFile.write(hgsqlGapOut)      
    gapFile.close()

    gapOverFile = str(db) + ".gapOver.bed"
    bedIntCmd = ["bedIntersect", "-aHitAny", gapFileName, tableBedTemp.name, gapOverFile]
    qaUtils.runCommand(bedIntCmd)
    
    gapOverUrls = constructOutputUrls(db, table, gapOverFile)
    gapOverlapOut += gapOverUrls 
    # Remove intermediate files
    os.remove(gapOverFile)
    os.remove(gapFileName)

    # If checkUnbridged is set, then also output overlap with unbridged gaps
    if checkUnbridged == True:
        # Create temp file to store unbridged gaps
        gapUnbrFileName = str(db) + ".gap.unbr.bed.temp"
        gapUnbrFile = open(gapUnbrFileName, 'w')
        # Get unbridged gaps
        for tbl in gapTableList:
                hgsqlGapOut = qaUtils.callHgsql(db, "select chrom, chromStart, chromEnd from " + tbl + " where bridge='no'")
                gapUnbrFile.write(hgsqlGapOut)
        gapUnbrFile.close()

        gapUnbrOverFile = str(db) + ".gapOver.unbr.bed"
        bedIntCmd = ["bedIntersect", "-aHitAny", gapUnbrFileName, tableBedTemp.name, gapUnbrOverFile]
        qaUtils.runCommand(bedIntCmd)

        gapUnbrOverUrls = constructOutputUrls(db, table, gapUnbrOverFile)
        gapOverlapOut += gapUnbrOverUrls 
        # Remove intermediate files
        os.remove(gapUnbrOverFile)
        os.remove(gapUnbrFileName)

    return gapOverlapOut
