#!/usr/bin/env python3

import sys,argparse
from collections import namedtuple,defaultdict

bedFields = ["chrom", "chromStart", "chromEnd", "name", "score", "strand", "thickStart",
 "thickEnd", "itemRgb", "_size", "mean_ratio", "genotype", "variant_class", "inheritance", "pathogenicity", "contribution", "phenotypes"]
geneFields = namedtuple("geneFields", bedFields[:4])

def setupCommandLine():
    parser = argparse.ArgumentParser(description="Read GVF lines from infile and transform to bed9+",
        add_help=True, usage = "%(prog)s [options]")
    parser.add_argument("bedFile", action="store", default=None, help="bed 16 file")
    parser.add_argument("geneList", action="store", default=None, help="knownCanonical gene list")
    args = parser.parse_args()
    if args.bedFile is "stdin" and args.geneList is stdin:
        sys.stderr.write("Error: only one of bedFile or geneList can be stdin.\n")
        parser.print_help()
        sys.exit(1)
    return args

def dumpBed(bed):
    print("#" + "\t".join(bedFields + ["Affected Genes", "_mouseOver"]))
    for patient in bed:
        for cnv in bed[patient]:
            ret = []
            for field in cnv:
                if "|" in field:
                    ret.append(", ".join(field.split("|")))
                else:
                    ret.append(field)
            print("\t".join(ret))

def getParentTerm(childTerm):
    lossTerms = ["Deletion"]
    structVarTerms = ["Amplification"]
    gainTerms = ["Copy-Number Gain", "Duplication", "Duplication/Triplication", "Triplication"]
    if childTerm in lossTerms:
        return "Loss"
    elif childTerm in structVarTerms:
        return "Structural alteration"
    elif childTerm in gainTerms:
        return "Gain"
    else:
        return "Unknown"

pathSteps = {
    1: ["benign", "likely benign"],
    2: ["unknown", "uncertain", "not provided", "others", "drug response"],
    3: ["likely pathogenic", "pathogenic"]
}

lossShades = {1: "255,128,128", 2: "255,0,0", 3: "153,0,0"}
gainShades = {1: "133,177,255", 2: "0,0,255", 3: "0,0,128"}
structVarShades = {1: "190,190,190", 2: "128,128,128", 3: "38,38,38"}

def getPathStep(pathogenicity):
    """Return how many levels we need to increase darkness"""
    for step in pathSteps:
        if pathogenicity.lower() in pathSteps[step]:
            return step
    return -1

def getColor(bed):
    varClass = getParentTerm(bed[12])
    pathogenicity = bed[14]
    pathStep = getPathStep(pathogenicity)
    if pathStep == -1:
        return "0,0,0"
    ret = ""
    if varClass == "Loss":
        ret = lossShades[pathStep]
    elif varClass == "Gain":
        ret = gainShades[pathStep]
    elif varClass == "Structural alteration":
        ret = structVarShades[pathStep]
    return ret
    
def getMouseOver(bed):
    ret = "Position: %s:%s-%s, Size: %s, Type: %s, " % (bed[0], int(bed[1])+1, bed[2], bed[9], bed[12])
    ret += "Significance: %s" % bed[14]
    phenList = bed[16].split("|")
    if phenList:
        ret += ", Phenotypes: %s" % (", ".join(phenList[:2]))
        if len(phenList) > 2:
            ret += ", others - click to see full list."
        else:
            ret += "."
    else:
        ret += ", Phenotypes: none."
    geneList = bed[17].split(", ")
    if geneList:
        ret += " Affected genes: %s" % (", ".join(geneList[:2]))
        if len(geneList) > 2:
            ret += ", others - click to see full list."
        else:
            ret += "."
    else:
        ret += ". Affected genes: none"
    return ret

def findOverlappingGenes(geneList, chrom, chromStart, chromEnd):
    ret = []
    for gene in geneList:
        if chrom == gene.chrom and \
            ((gene.chromStart <= chromStart and gene.chromEnd >= chromStart) or \
            (gene.chromStart >= chromStart and gene.chromStart <= chromEnd) or \
            (gene.chromStart >= chromStart and gene.chromEnd <= chromEnd)):
            ret.append(gene.name)
    ret = sorted(set(ret))
    return "%s" % (", ".join(ret))

def addGenesToBed(infile, geneList):
    inBed = defaultdict(list)
    infh = ""
    if infile == "stdin":
        infh = sys.stdin
    else:
        infh = open(infile)
    for line in infh:
        fields = line.strip('\n').split('\t')
        patientId = fields[3]
        overlapGenes = findOverlappingGenes(geneList, fields[0],int(fields[1]),int(fields[2]))
        bedPartial = fields[:8] + [getColor(fields)] + fields[9:] + [overlapGenes]
        inBed[patientId].append(bedPartial + [getMouseOver(bedPartial)])
    infh.close()
    return inBed

def readGeneList(infile):
    geneList = []
    if infile == "stdin":
        for line in sys.stdin:
            fields = line.strip('\n').split('\t')
            geneList.append(geneFields(chrom=fields[0],chromStart=int(fields[1]),chromEnd=int(fields[2]),name=fields[3]))
    else:
        with open(infile) as f:
            for line in f:
                fields = line.strip().split('\t')
                geneList.append(geneFields(chrom=fields[0],chromStart=int(fields[1]),chromEnd=int(fields[2]),name=fields[3]))
    return geneList

def main():
    args = setupCommandLine()
    inBed = args.bedFile
    inGenes = args.geneList
    geneList = readGeneList(inGenes)
    newBed = addGenesToBed(inBed, geneList)
    dumpBed(newBed)

if __name__ == "__main__":
    main()
