#!/usr/bin/env python3

"""
Turn ClinGen gene_curation_list and region_curation_list into
two bed files, one for haploinsufficient and one for triplosensitive
"""

import os,sys
import argparse

bed8Fields = ["chrom", "chromStart", "chromEnd", "name", "score", "strand", "thickStart",
    "thickEnd"]

# we only use some of these fields depending on whether we are working with haplo or triplo scores
commonFields = ["cytoBand", "Genomic Location", "Haploinsufficiency Score", "Haploinsufficiency Description", "Haploinsufficiency PMID1", "Haploinsufficiency PMID2", "Haploinsufficiency PMID3", "Triplosensitivity Score", "Triplosensitivity Description", "Triplosensitivity PMID1", "Triplosensitivity PMID2", "Triplosensitivity PMID3", "Date Last Evaluated", "Loss phenotype OMIM ID", "Triplosensitive phenotype OMIM ID"] #, 
haploFields = ["Haploinsufficiency Score", "Haploinsufficiency Description", "Haploinsufficiency PMID1", "Haploinsufficiency PMID2", "Haploinsufficiency PMID3"]
triploFields = ["Triplosensitivity Score", "Triplosensitivity Description", "Triplosensitivity PMID1", "Triplosensitivity PMID2", "Triplosensitivity PMID3"]
geneListFields = ["Gene Symbol", "Gene ID"]
regionListFields = ["ISCA ID", "ISCA Region Name"]
omimOutFields = ["OMIM ID", "OMIM Description"]

# the gene and region files get merged:
combinedFields = ["Gene Symbol/ISCA ID", "Gene ID/ISCA Region Name"]

clinGenGeneUrl = "https://www.ncbi.nlm.nih.gov/projects/dbvar/clingen/clingen_gene.cgi?sym="
clinGenIscaUrl = "https://www.ncbi.nlm.nih.gov/projects/dbvar/clingen/clingen_region.cgi?id="

scoreExplanation = {
    -1: "not yet evaluated",
    0: "no evidence for dosage pathogenicity",
    1: "little evidence for dosage pathogenicity",
    2: "some evidence for dosage pathogenicity",
    3: "sufficient evidence for dosage pathogenicity",
    30: "gene associated with autosomal recessive phenotype",
    40: "haploinsufficiency unlikely"
    }

# the input data keyed by "Gene Symbol" or "ISCA ID", and further keyed by each field name
inData = {}

# omim phenotype info
omimData = {}

def setupCommandLine():
    parser = argparse.ArgumentParser(description="Transform ClinGen region and gene lists into a bed file",
        add_help=True, usage = "%(prog)s [options]")
    parser.add_argument("regionFile", action="store", default=None, help="a ClinGen_region_curation_list file")
    parser.add_argument("geneFile", action="store", default=None, help="a ClinGen_gene_curation_list file")
    parser.add_argument("omimPhenotypesFile", action="store", default=None, help="two column tab-sep file with omim ids and phenotypes")
    parser.add_argument("outFileBase", action="store", default=None, help="the base file name for the two output files: outFileBase.haplo.bed and outFileBase.triplo.bed will be created.")
    args = parser.parse_args()
    if args.regionFile == "stdin" and args.geneFile == "stdin":
        print("Only one of regionFile or geneFile can be stdin")
        sys.exit(1)
    return args

def getColor(bed):
    """Return the shade of the item according to ClinGen rules. Red or blue for score 3, everything
        else gray."""
    retColors = {"haplo": "128,128,128", "triplo": "128,128,128"}
    if bed["Haploinsufficiency Score"] == "3": retColors["haplo"] = "255,0,0"
    if bed["Triplosensitivity Score"] == "3": retColors["triplo"] = "0,0,255"
    return retColors

def getMouseover(bed):
    """Return the mouseOver strings for this bed record."""
    hapScore = bed["Haploinsufficiency Score"]
    tripScore = bed["Triplosensitivity Score"]
    ret = {"haplo": "", "triplo": ""}
    mouseOver = "Gene/ISCA ID: %s" % bed["name"]
    ret["triplo"] = mouseOver + ", Triplosensitivity: "
    ret["triplo"] += "%s - %s" % (tripScore, scoreExplanation[int(tripScore)] if tripScore and tripScore != "Not yet evaluated" else -1)
    ret["haplo"] = mouseOver + ", Haploinsufficiency: "
    ret["haplo"] += "%s - %s" % (hapScore, scoreExplanation[int(hapScore)] if hapScore and hapScore != "Not yet evaluated" else -1)
    return ret

def parsePosition(posStr):
    """Turn a chr:start-stop string into bed chrom, chromStart, chromEnd"""
    try:
        colonDelim = posStr.split(":")
        chrom = colonDelim[0]
        dashDelim = colonDelim[1].split("-")
        start = int(dashDelim[0]) - 1
        end = int(dashDelim[1])
    except IndexError:
        sys.stderr.write("bad position string: %s\n" % posStr)
        sys.exit(1)
    return (chrom, start, end)

def makeBedLines():
    """Turn inData into bed 9+"""
    bedList = []
    for key in inData:
        data = inData[key]
        bed = {}
        chrom, start, end = parsePosition(data["Genomic Location"])
        bed["chrom"] = chrom
        bed["chromStart"] = start
        bed["chromEnd"] = end
        bed["name"] = key
        bed["score"] = 0
        bed["strand"] = "."
        bed["thickStart"] = bed["chromStart"]
        bed["thickEnd"] = bed["chromEnd"]
        bed["Size"] = bed["chromEnd"] - bed["chromStart"]
        bed["Gene Symbol/ISCA ID"] = key
        extra = inData[key]
        bed.update(extra)
        bed["itemRgb"] = getColor(bed) # dict of colors
        bed["_mouseOver"] = getMouseover(bed) # dict of mouseovers
        bedList.append(bed)
    return bedList

def dumpBedLines(ofBase):
    """For each line in makeBedLines(), write to two files: ofBase.haplo.bed and ofBase.triplo.bed"""
    bedLines = makeBedLines()
    combined = combinedFields + commonFields
    extraFields = ["clinGen URL"] + [x for x in combined if x != "Genomic Location"]

    # some fields differ depending on which file we are writing to, so remove them:
    for field in haploFields + triploFields + ["Loss phenotype OMIM ID", "Triplosensitive phenotype OMIM ID"]:
        extraFields.remove(field)

    if not ofBase.endswith("."):
        ofBase += "."
    haploOfh = ofBase + "haplo.bed"
    triploOfh = ofBase + "triplo.bed"
    haploFile = open(haploOfh, "w")
    triploFile = open(triploOfh, "w")
    print("#%s" % ("\t".join(bed8Fields + ["itemRgb"] + extraFields + omimOutFields + haploFields + ["_mouseOver"])), file=haploFile)
    print("#%s" % ("\t".join(bed8Fields + ["itemRgb"] + extraFields + omimOutFields + triploFields + ["_mouseOver"])), file=triploFile)
    for bed in bedLines:
        finalBed = []
        haploBed = []
        triploBed = []

        # first the standard bed fields
        for field in bed8Fields:
            if type(bed[field]) is list:
                haploBed.append(bed[field])
                triploBed.append(", ".join(bed[field]))
            else:
                haploBed.append(str(bed[field]))
                triploBed.append(str(bed[field]))

        # then the common extra fields:
        haploBed.append(str(bed["itemRgb"]["haplo"]))
        triploBed.append(str(bed["itemRgb"]["triplo"]))
        for field in extraFields:
            try:
                if field == "Gene ID/ISCA Region Name":
                    if "Gene ID" in bed:
                        haploBed.append(str(bed["Gene ID"]))
                        triploBed.append(str(bed["Gene ID"]))
                    else:
                        haploBed.append(str(bed["ISCA Region Name"]))
                        triploBed.append(str(bed["ISCA Region Name"]))
                    continue
                if type(bed[field]) is list:
                    haploBed.append(", ".join(bed[field]))
                    triploBed.append(", ".join(bed[field]))
                else:
                    haploBed.append(str(bed[field]))
                    triploBed.append(str(bed[field]))
            except KeyError:
                sys.stderr.write("ill formed bed:\n%s\n" % (bed))
                sys.exit(1)

        # the OMIM fields
        haploId = bed["Loss phenotype OMIM ID"]
        haploBed.append(str(haploId))
        if haploId and haploId in omimData:
                haploBed.append(str(omimData[haploId]))
        else:
            haploBed.append("")
        triploId = bed["Triplosensitive phenotype OMIM ID"]
        triploBed.append(str(triploId))
        if triploId and triploId in omimData:
            triploBed.append(str(omimData[triploId]))
        else:
            triploBed.append("")

        # then the differing extra fields:
        haploBed += [bed[field] for field in haploFields] + [bed["_mouseOver"]["haplo"]]
        triploBed += [bed[field] for field in triploFields] + [bed ["_mouseOver"]["triplo"]]
        print("\t".join(haploBed), file=haploFile)
        print("\t".join(triploBed), file=triploFile)
    haploFile.close()
    triploFile.close()

def parseOmim(infh):
    with open(infh) as f:
        for line in f:
            omimId,phenotype = line.strip().split('\t')
            omimData[omimId] = phenotype

def parseInFile(fname, fileType="gene"):
    if fname != "stdin":
        f = open(fname)
    lineNumber = 1
    for line in f:
        lineNumber += 1
        bizarreRecord = False
        if line.startswith("#"):
            continue
        l = line.strip("\n").split("\t")
        key = l[0] # geneSymbol or ISCA id
        extra = l[1:]
        if key in inData:   
            print("duplicate info for gene symbol: '%s'" % (geneSymbol))
            sys.exit(1)
        else:
            temp = {}
            fileFields = []
            if fileType == "gene":
                fileFields = geneListFields + commonFields
            else:
                fileFields = regionListFields + commonFields
            for keyName,ix in zip(fileFields[1:],range(len(fileFields[1:]))):
                try:
                    if keyName == "Genomic Location" and extra[ix] == "tbd":
                        bizarreRecord = True
                    temp[keyName] = extra[ix]
                except IndexError:
                    sys.stderr.write("error line %d or file: %s\n" % (lineNumber, fname))
                    sys.exit(1)
            if not bizarreRecord:
                if fileType == "gene":
                    temp["clinGen URL"] = clinGenGeneUrl + key
                else:
                    temp["clinGen URL"] = clinGenIscaUrl + key
                inData[key] = temp
    f.close()

def main():
    args = setupCommandLine()
    parseInFile(args.geneFile, "gene")
    parseInFile(args.regionFile, "region")
    parseOmim(args.omimPhenotypesFile)
    dumpBedLines(args.outFileBase)

if __name__=="__main__":
    main()
