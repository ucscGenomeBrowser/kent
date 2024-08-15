#!/bin/env python3
import sys
import json
from collections import namedtuple

# Take a decipher variants file and parse it into separate CNV and SNV track beds, while amending
# some of the field values to match our display uses.  For one thing, we change the colors.


def findOverlappingGenes(geneList, chrom, chromStart, chromEnd):
    ret = []
    for gene in geneList:
        if chrom == gene.chrom and \
            (gene.chromStart <= chromEnd and gene.chromEnd >= chromStart):
            ret.append(gene.name)
    ret = sorted(set(ret))
    return "%s" % (", ".join(ret))


# functions and variables to support working out which color to assign to each CNV variant

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

def getColor(varClass, pathogenicity):
    parentClass = getParentTerm(varClass)
    pathStep = getPathStep(pathogenicity)
    if pathStep == -1:
        return "0,0,0"
    ret = ""
    if parentClass == "Loss":
        ret = lossShades[pathStep]
    elif parentClass == "Gain":
        ret = gainShades[pathStep]
    elif parentClass == "Structural alteration":
        ret = structVarShades[pathStep]
    return ret


#############################################

def main():
    if len(sys.argv) != 6:
        print ("Usage: makeUcscBed.py <input file> <genefile> <cnv output file> <snv output file> <snv raw output file>")
        sys.exit(0)

    cnvOutput = open(sys.argv[3], "w")
    snvOutput = open(sys.argv[4], "w")
    snvRawOutput = open(sys.argv[5], "w")

    geneFields = namedtuple("geneFields", ["chrom", "chromStart", "chromEnd", "name"])
    geneList = []
    with open(sys.argv[2]) as geneFile:
        for line in geneFile:
            fields = line.strip('\n').split('\t')
            geneList.append(geneFields(chrom=fields[0],chromStart=int(fields[1]),chromEnd=int(fields[2]),name=fields[3]))

    with open(sys.argv[1], "r", encoding="utf8") as inputFile:
        header = inputFile.readline()
        for line in inputFile:
            fields = line.strip('\n').split('\t')
            parsedJson = json.loads(fields[13])

            varType = parsedJson["variant_type"];
            if (varType == "SNV") :
                # Make the basic table file
                result = "\t".join(fields[:4])
                print (result, file=snvOutput)

                # Make the "raw" data file with more fields
                # format: id, chrom, start, end, ref, alt, transcript, gene, genotype, inheritance, pathgenicity,
                    # contribution, phenotypes
                fields[0] = fields[0].replace("chr", "")  # strip off initial "chr" for these names
                # turn a 0-based start into 1-based for this specific output file for legacy reasons
                if (fields[1] != fields[2]) :
                    fields[1] = str(int(fields[1])+1)  
                result = fields[3] + "\t" + "\t".join(fields[:3])
                # phenotypes is itself a dictionary that we need to unpack; the current internal format
                # expectation is that the phenotypes are separated by | in the snvRaw file
                phenotypes = "|".join(sorted(x["name"] for x in parsedJson["phenotypes"]))
                transcript = parsedJson.get("transcript", "") # default to the empty string
                gene = parsedJson.get("gene", "") # default to the empty string
                result += "\t" + "\t".join( (parsedJson["ref_allele"], parsedJson["alt_allele"], transcript, \
                    gene, parsedJson["genotype"], parsedJson["inheritance"], parsedJson["pathogenicity"], \
                    parsedJson["contribution"], phenotypes) )
                print (result, file=snvRawOutput)
            else:
                # just sanity check here to ensure varType hasn't gone awry
                if (varType != "CNV"):
                    print ("Error: unexpected variant_type {} in input file\n".format(varType))
                    sys.exit(1)
                fields[5] = "."  # Input uses +, but these aren't directional items
                result = "\t".join(fields[:8])  # everything through thickEnd is fine
                # color is determined by variant_class
                varClass = parsedJson["variant_class"]
                pathogenicity = parsedJson["pathogenicity"]
                color = getColor(varClass, pathogenicity)
                result += "\t" + color
                # remaining fields to make: extent, mean ratio, genotype, variant_class, inheritance, pathogenicity,
                    # contribution, phenotypes, Affected Genes, mouseover text
                phenotypeString = ", ".join(sorted(x["name"] for x in parsedJson["phenotypes"]))
                meanRatio = parsedJson["mean_ratio"]
                if meanRatio is None:
                    meanRatio = "0.0"  # Not sure this is the best way to handle this case, but it's a start
                meanRatioString = "%0.2f" % float(meanRatio)
                result += "\t" + "\t".join( (fields[10], meanRatioString, parsedJson["genotype"], \
                    parsedJson["variant_class"], parsedJson["inheritance"], parsedJson["pathogenicity"], \
                    parsedJson["contribution"], phenotypeString) )

                # still have to do Affected Genes and the mouseover text
                geneString = findOverlappingGenes(geneList, fields[0], int(fields[1]), int(fields[2]))
                result += "\t" + geneString

                mouseOver = "Position: %s:%s-%s, Size: %s, Type: %s, " % (fields[0], int(fields[1])+1, \
                        fields[2], fields[10], varClass)
                mouseOver += "Significance: %s" % pathogenicity
                phenotypes = sorted(x["name"] for x in parsedJson["phenotypes"])
                if len(phenotypes) > 0:
                    mouseOver += ", Phenotypes: %s" % (", ".join(phenotypes[:2]))
                    if len(phenotypes) > 2:
                        mouseOver += ", others - click to see full list."
                    else:
                        mouseOver += "."
                else:
                    mouseOver += ", Phenotypes: none."
                thisGeneList = geneString.split(", ")
                if thisGeneList:
                    mouseOver += " Affected genes: %s" % (", ".join(thisGeneList[:2]))
                    if len(thisGeneList) > 2:
                        mouseOver += ", others - click to see full list."
                    else:
                        mouseOver += "."
                else:
                    mouseOver += ". Affected genes: none"

                result += "\t" + mouseOver
                print (result, file=cnvOutput)

    cnvOutput.close()
    snvOutput.close()
    snvRawOutput.close()

if __name__ == "__main__":
    main()
