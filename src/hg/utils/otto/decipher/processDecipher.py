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
    1: ["unknown", "not provided", "others", "drug response"],
    2: ["benign", "likely benign"],
    3: ["protective", "conflicting", "affects"],
    4: ["uncertain", "association", "risk factor"],
    5: ["likely pathogenic", "pathogenic"]
}
lossShades = {1: "247,188,187", 2: "238,146,148", 3: "232,104,111", 4: "218,44,55", 5: "180,3,16"}
gainShades = {1: "161,208,232", 2: "122,165,211", 3: "88,131,211", 4: "41,78,174", 5: "17,44,138"}
structVarShades = {1: "166,235,182", 2: "96,208,121", 3: "47,172,76", 4: "6,104,28", 5: "1,69,17"}

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
        sys.stderr.write("ERROR: unsupported pathogenicty: '%s'\n" % pathogenicity)
        sys.exit(1)
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
