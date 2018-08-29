import stat
import os
        
from ucscGb.qa import qaUtils
from ucscGb.qa.tables import trackUtils

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

        vis = trackUtils.checkCanPack(db, table)

        # This block of code should deal with most configurations of views + composites
        # (composites w/o views + composites w/ views)
        parentUrl = ""
        parent = trackUtils.getParent(db, table)
        view = trackUtils.getAttributeForTrack("view", db, table)
        if view:
            # If a composite contains views, the direct parent of the track will be the view.
            # Changing the view visibility does nothing, so we need to grab the view's parent.
            viewParent = trackUtils.getParent(db, parent)
            viewParentVis = trackUtils.checkCanPack(db, viewParent)
            parentUrl = "&" + viewParent + "=" + viewParentVis
        elif parent:
            parentVis = trackUtils.checkCanPack(db, table)
            parentUrl = "&" + parent + "=" + parentVis

        superTrackUrl = ""
        superTrack = trackUtils.getAttributeForTrack("superTrack", db, table)
        if superTrack:
            superTrackSplit = superTrack.strip().split(" ")
            superTrackName = superTrackSplit[0]
            superTrackUrl = "&" + superTrackName + "=show"

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
                # Order of tracks in hgTracks URLs is very sensitive. It MUST be in this order:
                # superTrack vis > composite vis > base track vis
                url="http://genome-test.soe.ucsc.edu/cgi-bin/hgTracks?db=" + db + "&hideTracks=1"\
                     + superTrackUrl + parentUrl + "&" + table + "=" + vis + "&gap=pack&position="\
                     + position + color + "\n"
                gapOverUrls += url  
                i+=1  
        outputUrls += str(lineCount) + " " + overlapFile + "\n\n" + gapOverUrls
        openOverlapFile.close()
    return outputUrls

def checkGapOverlap(db, table, checkUnbridged=False):
    """Check if any items in a table over lap with bridged and unbridged gaps"""
    tableBed = trackUtils.makeBedFromTable(db, table)
    tableFileName = db + "." + table + ".bed.temp"
    tableFile = open(tableFileName, "w")
    tableFile.write(tableBed)
    tableFile.close()

    gapOverlapOut = ""
    gapTableList = trackUtils.checkSplit(db, "gap")
    gapFileName = str(db) + ".gap.bed.temp"
    gapFile = open(gapFileName, "a")
    for tbl in gapTableList:
            gapBed = trackUtils.makeBedFromTable(db, "gap")
            gapFile.write(gapBed)
    gapFile.close()

    gapOverFile = str(db) + ".gapOver.bed"
    bedIntCmd = ["bedIntersect", "-aHitAny", gapFileName, tableFileName, gapOverFile]
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
        gapUnbrFile = open(gapUnbrFileName, "a")
        # Get unbridged gaps
        for tbl in gapTableList:
                gapUnbrBed = trackUtils.makeBedFromTable(db, "gap", whereOpt="bridge='no'")
                gapUnbrFile.write(gapUnbrBed)
        gapUnbrFile.close()

        gapUnbrOverFile = str(db) + ".gapOver.unbr.bed"
        bedIntCmd = ["bedIntersect", "-aHitAny", gapUnbrFileName, tableFileName, gapUnbrOverFile]
        qaUtils.runCommand(bedIntCmd)

        gapUnbrOverUrls = constructOutputUrls(db, table, gapUnbrOverFile)
        gapOverlapOut += gapUnbrOverUrls 
        # Remove intermediate files
        os.remove(gapUnbrOverFile)
        os.remove(gapUnbrFileName)
    os.remove(tableFileName)

    return gapOverlapOut
