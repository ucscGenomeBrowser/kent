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
commonFields = ["cytoBand", "Genomic Location", "Haploinsufficiency Score", "Haploinsufficiency Description", "Haploinsufficiency PMID1", "Haploinsufficiency PMID2", "Haploinsufficiency PMID3", "Haploinsufficiency PMID4", "Haploinsufficiency PMID5", "Haploinsufficiency PMID6", "Triplosensitivity Score", "Triplosensitivity Description", "Triplosensitivity PMID1", "Triplosensitivity PMID2", "Triplosensitivity PMID3", "Triplosensitivity PMID4", "Triplosensitivity PMID5", "Triplosensitivity PMID6", "Date Last Evaluated", "Haploinsufficiency Disease ID", "Triplosensitivity Disease ID"] #, 
haploFields = ["Haploinsufficiency Score", "Haploinsufficiency Description", "Haploinsufficiency PMID1", "Haploinsufficiency PMID2", "Haploinsufficiency PMID3", "Haploinsufficiency PMID4", "Haploinsufficiency PMID5", "Haploinsufficiency PMID6"]
triploFields = ["Triplosensitivity Score", "Triplosensitivity Description", "Triplosensitivity PMID1", "Triplosensitivity PMID2", "Triplosensitivity PMID3", "Triplosensitivity PMID4", "Triplosensitivity PMID5", "Triplosensitivity PMID6"]
geneListFields = ["Gene Symbol", "Gene ID"]
regionListFields = ["ISCA ID", "ISCA Region Name"]
omimOutFields = ["OMIM ID", "OMIM Description"]
mondoOutFields = ["MONDO ID"]

# the gene and region files get merged:
combinedFields = ["Gene Symbol/ISCA ID", "Gene ID/ISCA Region Name"]

clinGenGeneUrl = "https://search.clinicalgenome.org/kb/gene-dosage/"
clinGenIscaUrl = "https://search.clinicalgenome.org/kb/gene-dosage/region/"

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

def setupCommandLine():
    parser = argparse.ArgumentParser(description="Transform ClinGen region and gene lists into a bed file",
        add_help=True, usage = "%(prog)s [options]")
    parser.add_argument("regionFile", action="store", default=None, help="a ClinGen_region_curation_list file")
    parser.add_argument("geneFile", action="store", default=None, help="a ClinGen_gene_curation_list file")
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
        name = key.split('\t')[0]
        data = inData[key]
        bed = {}
        chrom, start, end = parsePosition(data["Genomic Location"])
        bed["chrom"] = chrom
        bed["chromStart"] = start
        bed["chromEnd"] = end
        bed["name"] = name
        bed["score"] = 0
        bed["strand"] = "."
        bed["thickStart"] = bed["chromStart"]
        bed["thickEnd"] = bed["chromEnd"]
        bed["Size"] = bed["chromEnd"] - bed["chromStart"]
        bed["Gene Symbol/ISCA ID"] = name
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
    for field in haploFields + triploFields + ["Haploinsufficiency Disease ID", "Triplosensitivity Disease ID"]:
        extraFields.remove(field)

    if not ofBase.endswith("."):
        ofBase += "."
    haploOfh = ofBase + "haplo.bed"
    triploOfh = ofBase + "triplo.bed"
    haploFile = open(haploOfh, "w")
    triploFile = open(triploOfh, "w")
    print("#%s" % ("\t".join(bed8Fields + ["itemRgb"] + extraFields + omimOutFields + haploFields + ["_mouseOver"] + mondoOutFields )), file=haploFile)
    print("#%s" % ("\t".join(bed8Fields + ["itemRgb"] + extraFields + omimOutFields + triploFields + ["_mouseOver"] + mondoOutFields )), file=triploFile)
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

        # The empty OMIM fields for preserving the old schema
        haploBed += ["",""]
        triploBed += ["",""]

        # then the differing extra fields:
        haploBed += [bed[field] for field in haploFields] + [bed["_mouseOver"]["haplo"]]
        triploBed += [bed[field] for field in triploFields] + [bed ["_mouseOver"]["triplo"]]

        # the MONDO field
        haploMondoId = bed["Haploinsufficiency Disease ID"]
        haploBed.append(str(haploMondoId))
        triploId = bed["Triplosensitivity Disease ID"]
        triploBed.append(str(triploId))
        print("\t".join(haploBed), file=haploFile)
        print("\t".join(triploBed), file=triploFile)
    haploFile.close()
    triploFile.close()

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
        sym = l[0] # geneSymbol or ISCA id
        extra = l[1:]
        pos = l[3] # Genomic location for uniqifying keys
        key = sym+ "\t" + pos
        if key in inData:   
            print("duplicate info for gene symbol: '%s'" % (sym))
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
                    temp["clinGen URL"] = clinGenGeneUrl + sym
                else:
                    temp["clinGen URL"] = clinGenIscaUrl + sym
                inData[key] = temp
    f.close()

def main():
    args = setupCommandLine()
    parseInFile(args.geneFile, "gene")
    parseInFile(args.regionFile, "region")
    dumpBedLines(args.outFileBase)

if __name__=="__main__":
    main()
