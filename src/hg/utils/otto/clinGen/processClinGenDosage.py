#!/usr/bin/env python3

"""
Read GVF lines from stdin and write bed9+ lines to stdout
"""

import os,sys,re
import argparse

bed9Fields = ["chrom", "chromStart", "chromEnd", "name", "score", "strand", "thickStart",
    "thickEnd", "itemRgb"]

# we only use some of these fields depending on whether we are working with haplo or triplo scores
extraFieldList = ["Gene Symbol", "Gene ID", "cytoBand", "Genomic Location",
    "Haploinsufficiency Score", "Haploinsufficiency Description", "Haploinsufficiency PMID1",
    "Haploinsufficiency PMID2", "Haploinsufficiency PMID3", "Triplosensitivity Score",
    "Triplosensitivity Description", "Triplosensitivity PMID1", "Triplosensitivity PMID2",
    "Triplosensitivity PMID3", "Date Last Evaluated", "Loss phenotype OMIM ID",
    "Triplosensitive phenotype OMIM ID"]

scoreExplanation = {
    -1: "not yet evaluated",
    0: "no evidence for dosage pathogenicity",
    1: "little evidence for dosage pathogenicity",
    2: "some evidence for dosage pathogenicity",
    3: "sufficient evidence for dosage pathogenicity",
    30: "gene associated with autosomal recessive phenotype",
    40: "haploinsufficiency unlikely"
    }

# hold all the bedlines in memory until the end as different lines have 
# different numbers of extra fields
bedLines = {}

# the phenotypes and other stuff associated with a particular gene
extraInfo = {}

def setupCommandLine():
    parser = argparse.ArgumentParser(description="Read bed lines from infile, join with extraFile and transform to bed9+",
        add_help=True, usage = "%(prog)s [options]")
    parser.add_argument("infile", action="store", default=None, help="Input bed file from which to read input, use 'stdin' to read from default stdin")
    parser.add_argument("extraFile", action="store", default=None, help="a ClinGen_gene_curation_list file with extra info for the records in the infile")
    parser.add_argument("dosageType", action="store", default=None, help="haplo or triplo, which extraFields to use for this bed record")
    args = parser.parse_args()
    if args.infile == "stdin" and args.extraFile == "stdin":
        print("Only one of infile or extraFile can be stdin")
        sys.exit(1)
    return args

def parseExtraFile(fname, dosageType):
    global extraInfo
    oppMatchStr = "triplo" if dosageType == "haplo" else "haplo|loss"
    fieldMatch = re.compile(oppMatchStr,re.I)
    with open(fname) as f:
        for line in f:
            if line.startswith("#"):
                continue
            l = line.strip("\n").split("\t")
            geneSymbol = l[0]
            extra = l[1:]
            try:
                if extraInfo[geneSymbol]:   
                    print("duplicate info for gene symbol: '%s'" % (geneSymbol))
                    sys.exit(1)
            except KeyError:
                extraInfo[geneSymbol] = {}
                for keyName,ix in zip(extraFieldList[1:],range(len(extraFieldList[1:]))):
                    try:
                        if fieldMatch.match(keyName):
                            continue
                        extraInfo[geneSymbol][keyName] = extra[ix]
                    except IndexError:
                        sys.exit(1)

def getColor(bed, dosageType):
    """Return the shade of the item according to it's variant type."""
    if dosageType == "haplo":
        if bed["score"] == -1: return "218,215,215"
        if bed["score"] == 0: return "130,128,128"
        if bed["score"] == 1: return "232,104,111"
        if bed["score"] == 2: return "218,44,55"
        if bed["score"] == 3: return  "180,3,16"
        if bed["score"] == 30: return "137,93,189"
        if bed["score"] == 40: return "238,146,148"
    if dosageType == "triplo":
        if bed["score"] == -1: return "218,215,215"
        if bed["score"] == 0: return "130,128,128"
        if bed["score"] == 1: return "88,131,211"
        if bed["score"] == 2: return "41,78,174"
        if bed["score"] == 3: return "17,44,138"
        if bed["score"] == 30: return "137,93,189"
        if bed["score"] == 40: return "122,165,211"

def getMouseover(bed, dosageType):
    """Return the mouseOver string for this bed record."""
    ret = "Gene: %s" % bed["name"]
    if dosageType == "triplo":
        ret += ", Triplosensitivity: "
    else:
        ret += ", Haplosensitivity: "
    ret += "%s - %s" % (bed["score"], scoreExplanation[bed["score"]])
    return ret

def dumpBedLines(dosageType):
    """Write out the bed lines, with the same number of fields in each line."""
    oppMatchStr = "triplo" if dosageType == "haplo" else "haplo|loss"
    extraFields = [x for x in extraFieldList if not re.match(oppMatchStr, x, re.I) and x != "Gene Symbol" and x != "Genomic Location"]
    fields = bed9Fields + extraFields+ ["_mouseOver"]
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

def makeBedLine(chrom, chromStart, chromEnd, name, score, dosageType):
    """Turn a single gvf line into a bed 9+ line."""
    bed = {}
    bed["chrom"] = chrom
    bed["chromStart"] = chromStart
    bed["chromEnd"] = chromEnd
    bed["name"] = name
    bed["score"] = score
    bed["strand"] = "."
    bed["thickStart"] = chromStart
    bed["thickEnd"] = chromEnd
    bed["itemRgb"] = getColor(bed, dosageType)
    bed["Size"] = chromEnd - chromStart
    extra = extraInfo[name]
    bed.update(extra)
    bed["_mouseOver"] = getMouseover(bed,dosageType)
    return bed

def processClinGenDosage(inf, dosageType):
    global bedLines
    lineCount = 1
    for line in inf:
        if line.startswith('#') or line.startswith('track') or line.startswith('browser'):
            lineCount += 1
            continue
        trimmed = line.strip()
        try:
            chrom, chromStart, chromEnd, name, score = trimmed.split("\t")
        except ValueError:
            sys.stderr.write("Error: ignoring ill formatted bed line %s:%d\n" % (inf.name, lineCount))
            lineCount += 1
            continue
        try:
            bedLines[name] = makeBedLine(chrom, int(chromStart), int(chromEnd), name, int(score), dosageType)
        except:
            # error here comes from something to do with the associated gene_curation_list and not
            # the dosage file itself
            if score.startswith("Not"):
                bedLines[name] = makeBedLine(chrom, int(chromStart), int(chromEnd), name, -1, dosageType)
            else:
                print(sys.exc_info()[0])
                sys.stderr.write("bad input line:\n%s\n" % line)
                sys.exit(1)
        lineCount += 1
    dumpBedLines(dosageType)

def main():
    args = setupCommandLine()
    if args.extraFile:
        parseExtraFile(args.extraFile, args.dosageType)
    if args.infile == "stdin":
        processClinGenDosage(sys.stdin,args.dosageType)
    else:
        with open(args.infile) as inf:
            processClinGenDosage(inf, args.dosageType)

if __name__=="__main__":
    main()
