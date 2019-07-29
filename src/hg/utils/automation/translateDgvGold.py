#!/bin/env python
"""
Parses the Gold-standard copy number variants GFF from DGV and writes
a tab separated bed 12 with extra fields to stdout.

Example:
./translateDgvGold.py input.gff | sort -k1,1 -k2,2n | uniq > output.bed12
"""

import sys

header = "chrom\tchromStart\tchromEnd\tname\tscore\tstrand\tthickStart\tthickEnd\t" \
    "itemRgb\tblockCount\tblockSizes\tblockStarts"

# gain blue, loss red
colors = {"Gain": "0,0,200", "Loss": "200,0,0"}

def parseExtraInfo(extraInfoList, doHeader=False):
    """ Parses the 9th field of the DGV Gold GFF and optionally assigns the header."""
    info = []
    extraHeader = []
    for x in extraInfoList:
        s = x.split("=")
        if doHeader:
            extraHeader.append(s[0])
        info.append(s[1])

    name = info[1]
    subType = info[3]
    outerStart = info[4]
    innerStart = info[5]
    innerEnd = info[6]
    outerEnd = info[7]

    # delete fields that are gonna be in the normal bed fields
    info.remove(name)
    info.remove(outerStart)
    info.remove(innerStart)
    info.remove(innerEnd)
    info.remove(outerEnd)
    extraString = "\t".join(info)
    extraString = extraString.replace(",", ", ")
    extraString = extraString.replace(":", ", ")

    if doHeader:
        global header
        extraHeader.remove("Name")
        extraHeader.remove("outer_start")
        extraHeader.remove("inner_start")
        extraHeader.remove("inner_end")
        extraHeader.remove("outer_end")
        header += "\t" + "\t".join(extraHeader)

    return name, subType, outerStart, innerStart, innerEnd, outerEnd, extraString

### main ###

if len(sys.argv) != 2:
    print("Usage: translateDgvGold.py input.gff | sort -k1,1 -k2,2n | uniq > out.bed12\n"\
            "\nWrites a bed12 to stdout.\n")
    sys.exit(1)

doHeader = True

for line in open(sys.argv[1]):
    gffFields = line.strip().split("\t")
    extraInfo = gffFields[8].split(";")
    chrom = gffFields[0]
    name, subType, outerStart, innerStart, innerEnd, chromEnd, extraString = parseExtraInfo(extraInfo, doHeader)
    if subType not in colors:
        sys.exit("CNV subtype: %s not recognized.\n" % subType)

    chromStart = str(int(outerStart) - 1)

    # one block
    if innerStart == outerStart and innerEnd == chromEnd:
        blockCount = "1"
        blockSizes = str(int(chromEnd) - int(chromStart))
        blockStarts = "0"
    # no left intron 
    elif outerStart == innerStart:
        blockCount = "2"
        blockStarts = "0," + str(int(chromEnd) - int(chromStart) - 1)
        blockSizes = str(int(innerEnd) - int(chromStart)) + ",1"
    # no right intron
    elif innerEnd == chromEnd:
        blockCount = "2"
        blockStarts = "0," + str(int(innerEnd) - (int(innerEnd) - int(innerStart)) - int(chromStart))
        blockSizes = "1," + str(int(innerEnd) - int(innerStart))
    # two introns
    else:
        blockCount = "3"
        blockSizes = "1,"+ str(int(innerEnd) - int(innerStart))+",1"
        blockStarts = "0," + \
            str(int(innerStart)-int(chromStart)) + \
            ","+str(int(chromEnd)-int(chromStart)-1)

    bedLine = [chrom,
        chromStart,
        chromEnd,
        name,
        "0",
        ".",
        chromEnd,
        chromEnd,
        colors[subType],
        blockCount,
        blockSizes,
        blockStarts,
        extraString]

    if doHeader:
        print("#" + header)

    print("\t".join(bedLine))
    doHeader = False
