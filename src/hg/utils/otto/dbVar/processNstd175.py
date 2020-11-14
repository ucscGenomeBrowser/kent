#!/usr/bin/env python3

"""
Read GVF lines from stdin and write bed9+ lines to stdout
"""

import os,sys,re
import argparse

bed9Fields = ["chrom", "chromStart", "chromEnd", "name", "score", "strand", "thickStart",
    "thickEnd", "itemRgb"]

# the following list of fields may not exist for every record in the input file
extraFieldList = ["Size", "Variant Type", "Variant Region", "Link to dbVar", "Sample Name", "Sampleset Name", "Phenotype"]

bedLines = {}
chromLift = {}

def setupCommandLine():
    parser = argparse.ArgumentParser(description="Read GVF lines from infile and transform to bed9+",
        add_help=True, usage = "%(prog)s [options]")
    parser.add_argument("infile", action="store", default=None, help="Input GVF file from which to read input, use 'stdin' to read from default stdin")
    parser.add_argument("liftFile", action="store", default=None, help="liftUp file for converting chrom names to UCSC style names")
    args = parser.parse_args()
    return args

def parseLiftFile(fname):
    global chromLift
    with open(fname) as f:
        for line in f:
            l = line.strip().split()
            ncbiChrom = l[1]
            ucscChrom = l[3]
            chromLift[ncbiChrom] = ucscChrom

def getColor(cnvtype=None):
    """Return the shade of the item according to it's variant type."""
    if cnvtype == "copy_number_variation":
        return "128,128,128"
    elif cnvtype == "deletion":
        return "255,0,0"
    elif cnvtype == "delins":
        return "255,0,0"
    elif cnvtype == "insertion":
        return "0,0,255"
    else:
        return "0,0,0"

def getMouseover(bed):
    """Return the mouseOver string for this bed record."""
    ret = "Position: %s:%s-%s" % (bed["chrom"], int(bed["chromStart"])+1, bed["chromEnd"])
    ret += ", Size: %d" % (int(bed["chromEnd"]) - int(bed["chromStart"]))
    ret += ", Variant Type: %s" % (bed["Variant Type"])
    if "Phenotype" in bed:
        ret += ", Phenotype: %s" % bed["Phenotype"]
    return ret

def dumpBedLines():
    """Write out the bed lines, with the same number of fields in each line."""
    fields = bed9Fields +  extraFieldList + ["_mouseOver"]
    print("#%s" % ("\t".join(fields)))
    for b in bedLines:
        bed = bedLines[b]
        finalBed = []
        for field in fields:
            try:
                if type(bed[field]) is list:
                    finalBed.append(", ".join(bed[field]))
                else:
                    finalBed.append(str(bed[field]))
            except KeyError: # some of the extra fields won't exist for every record
                finalBed.append("")
        print("\t".join(finalBed))

def processExtraFields(extraFields):
    """Special processing of the GVF extra fields"""
    ret = {}
    for key in extraFields:
        val = extraFields[key]
        if key == "ID" or key == "Name" or key == "Variant Seq" or key == "Reference Seq":
            continue
        elif key == "Dbxref":
            splitxrefs = val.split(',')
            if len(splitxrefs) > 1:
                sys.stderr.write("Error: more dbXref fields this release:\n")
                sys.stderr.write("%s\n" % (extraFields))
                sys.stderr.write("stopping\n")
                sys.exit(1)
            else:
                val = "https://" + val[4:]
                ret["Link to dbVar"] = val
        elif key == "Parent":
            ret["Variant Region"] = val
        else:
            ret[key] = val
    return ret

def makeBedLine(chrom, chromStart, chromEnd, bedId, extraHash):
    """Turn a single gvf line into a bed 9+ line."""
    bed = {}
    bed["chrom"] = chromLift[chrom]
    bed["chromStart"] = chromStart
    bed["chromEnd"] = chromEnd
    bed["name"] = extraHash["Name"]
    bed["score"] = 0
    bed["strand"] = "."
    bed["thickStart"] = chromStart
    bed["thickEnd"] = chromEnd
    extra = processExtraFields(extraHash)
    bed.update(extra)
    bed["itemRgb"] = getColor(bed["Variant Type"])
    bed["_mouseOver"] = getMouseover(bed)
    bed["Size"] = chromEnd - chromStart
    return bed

def fixupFieldName(key):
    """Capitalize field names and turn '_' to space."""
    return " ".join(s[0].upper() + s[1:] for s in key.replace('_', ' ').split())

def processNstd175(inf):
    global bedLines
    for line in inf:
        if line.startswith('#') or line.startswith('track') or line.startswith('browser'):
            continue
        trimmed = line.strip()
        fields = trimmed.split(maxsplit=8)
        extraFields = fields[-1].split(';')
        itemName = extraFields[1].split('=')[1]
        extraHash = {}
        for f in extraFields:
            k,v = f.strip().split('=')
            fixedName = fixupFieldName(k)
            extraHash[fixedName] = v
        bedId = itemName
        extraHash["Variant Type"] = fields[2]
        try:
            bedLines[bedId] = makeBedLine(fields[0], int(fields[3]) - 1, int(fields[4]), bedId, extraHash)
        except KeyError as e:
            sys.stderr.write("KeyError\n")
            sys.stderr.write("Offending line: %s\n" % line)
            sys.exit(1)
    dumpBedLines()

def main():
    args = setupCommandLine()
    if args.liftFile:
        parseLiftFile(args.liftFile)
    if args.infile == "stdin":
        processNstd175(sys.stdin)
    else:
        with open(args.infile) as inf:
            processNstd175(inf)

if __name__=="__main__":
    main()
