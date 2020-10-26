#!/usr/bin/env python3

"""
Read GVF lines from stdin and write bed9+ lines to stdout
"""

import os,sys,re
import argparse

bed9Fields = ["chrom", "chromStart", "chromEnd", "name", "score", "strand", "thickStart",
    "thickEnd", "itemRgb"]

# the following list of fields may not exist for every record in the input file
extraFieldList= ["Size", "Alias", "Variant Region", "Start Range", "End Range", "Copy Number", "Variant Type",
    "OMIM", "ClinGen", "Phenotype", "Phenotype Id", "PubMed", "Clinical Interpretation",
    "Clinical Source", "Sample Name", "Sampleset Name"] #, "Variant Seq", "Reference Seq", "RemapScore"]
#extraFieldsSet = set()

# hold all the bedlines in memory until the end as different lines have 
# different numbers of extra fields
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

def getColor(cnvtype, pathOrBen):
    """Return the shade of the item according to it's variant type."""
    if cnvtype not in ["copy_number_loss","copy_number_gain"] or pathOrBen not in ["Benign", "Pathogenic"]:
        return "0,0,0"
    if cnvtype == "copy_number_loss":
        if pathOrBen == "Pathogenic":
            return "180,3,16"
        else:
            return "238,146,148"
    if cnvtype == "copy_number_gain":
        if pathOrBen == "Pathogenic":
            return "17,44,138"
        else:
            return "122,165,211"

def getMouseover(bed):
    """Return the mouseOver string for this bed record."""
    ret = ""
    ret += "Gene(s) affected: %s" % (", ".join(bed["ClinGen"]))
    ret += ", Position: %s:%s-%s" % (bed["chrom"], int(bed["chromStart"])+1, bed["chromEnd"])
    ret += ", Size: %d" % (int(bed["chromEnd"]) - int(bed["chromStart"]))
    if bed["Variant Type"] == "copy_number_loss":
        ret += ", Type: loss"
    else:
        ret += ", Type: gain"
    ret += ", Significance: %s" % bed["Clinical Interpretation"]
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
                    if field == "ClinGen":
                        urls = ""
                        for ix in range(len(bed[field])):
                            item = bed[field][ix]
                            if item.startswith("ISCA"):
                                urls += "https://dosage.clinicalgenome.org/clingen_region.cgi?id=" + item
                            else:
                                urls += "https://dosage.clinicalgenome.org/clingen_gene.cgi?sym=" + item
                            if (ix + 1) < len(bed[field]):
                                urls += ", "
                        finalBed.append(urls)
                    else:
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
        if key == "ID" or key == "Name":
            continue
        elif key == "Start Range" or key == "End Range":
            r = val.split(",")
            if len(r) != 2:
               ret[key] = val
            else:
                rangeStart = r[0]
                rangeEnd = r[1]
                if rangeStart == ".":
                    rangeStart = "unknown"
                if rangeEnd == ".":
                    rangeEnd = "unknown"
                if key == "Start Range":
                    ret[key] = "outer start %s, inner start %s" % (rangeStart, rangeEnd)
                else:
                    ret[key] = "inner end %s, outer end %s" % (rangeStart, rangeEnd)
        elif key == "Dbxref":
            splitxrefs = val.split(',')
            for xref in splitxrefs:
                db,v = xref.split(':')
                if db in ret:
                    ret[db].append(v)
                else:
                    ret[db] = [v]
        elif key == "Parent":
            ret["Variant Region"] = val
        else:
            ret[key] = val
    return ret

def makeBedLine(chrom, chromStart, chromEnd, bedId, extraHash):
    """Turn a single gvf line into a bed 9+ line."""
    #global extraFieldsSet
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
    #extraFieldsSet |= extra.keys()
    bed["itemRgb"] = getColor(bed["Variant Type"], bed["Clinical Interpretation"])
    bed["_mouseOver"] = getMouseover(bed)
    bed["Size"] = chromEnd - chromStart
    return bed

def fixupFieldName(key):
    """Capitalize field names and turn '_' to space."""
    if key == "clinical_int":
        return "Clinical Interpretation"
    return " ".join(s[0].upper() + s[1:] for s in key.replace('_', ' ').split())

def processClinGenCnv(inf):
    global bedLines
    extraHash = {}
    # only keep "ssv" records, which effectively ignores the variant_region file
    nameMatch = re.compile("^[ne]sv\d")
    for line in inf:
        if line.startswith('#') or line.startswith('track') or line.startswith('browser'):
            continue
        trimmed = line.strip()
        fields = trimmed.split(maxsplit=8)
        extraFields = fields[-1].split(';')
        itemName = extraFields[1].split('=')[1]
        if nameMatch.match(itemName):
            continue
        for f in extraFields:
            k,v = f.strip().split('=')
            fixedName = fixupFieldName(k)
            #if fixedName not in extraFieldsSet:
            #    extraFieldsSet.add(fixedName)
            extraHash[fixedName] = v
        bedId = itemName
        extraHash["Variant Type"] = fields[2]
        bedLines[bedId] = makeBedLine(fields[0], int(fields[3]) - 1, int(fields[4]), bedId, extraHash)
    dumpBedLines()

def main():
    args = setupCommandLine()
    if args.liftFile:
        parseLiftFile(args.liftFile)
    if args.infile == "stdin":
        processClinGenCnv(sys.stdin)
    else:
        with open(args.infile) as inf:
            processClinGenCnv(inf)

if __name__=="__main__":
    main()
