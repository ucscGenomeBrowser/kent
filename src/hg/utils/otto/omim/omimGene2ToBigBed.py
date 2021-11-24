# convert the omimGene2 sql table format to bigBed format with a mouse over string
import sys, os

def inhModeToCode(inhModes):
    inhCodes = []
    inhModes = inhModes.split(", ")
    for inhMode in inhModes:
        inhMode = inhMode.strip("?")
        if inhMode == "":
            inhCode = ""
        elif inhMode == "Autosomal dominant":
            inhCode = "AD"
        elif inhMode == "Autosomal recessive":
            inhCode = "AR"
        elif inhMode == "X-linked":
            inhCode = "XL"
        elif inhMode =="X-linked dominant":
            inhCode = "XLD"
        elif inhMode == "X-linked recessive":
            inhCode = "XLR"
        elif inhMode == "Y-linked":
            inhCode = "YL"
        #elif inhMode == "Multifactorial":
        #    inhCode = "Mu"
        #elif inhMode == "Somatic Mutation" or inhMode=="Somatic mutation":
        #    inhCode = "SMu"
        #elif inhMode == "Isolated cases":
        #    inhCode = "IC"
        #elif inhMode == "Digenic dominant":
        #    inhCode = "DD"
        #elif inhMode == "Digenic Recessive":
        #    inhCode = "DR"
        #elif inhMode == "Mitochondrial":
        #    inhCode = "Mi"
        #elif inhMode == "Somatic mosaicism":
        #    inhCode = "SomMos"
        #elif inhMode =="Pseudoautosomal recessive":
        #    inhCode = "PARec"
        #elif inhMode =="Pseudoautosomal dominant":
        #    inhCode = "PADom"
        ##elif inhMode == "Autosomal recessive, Autosomal dominant":
        #    #inhCode = "AR/AD"
        ##elif inhMode =="Somatic mutation, Autosomal dominant":
        #    #inhCode = "SMu/AD"
        #elif inhMode =="Digenic recessive":
        #    inhCode = "DigRec"
        ##elif inhMode == "Isolated cases, Autosomal dominant":
        #    #inhCode = "AD"
        ##elif inhMode == 'Multifactorial, Autosomal recessive, Autosomal dominant':
        #    #inhCode = "Mu/AR/AD"
        else:
            #print(repr(inhMode))
            #assert(False)
            inhCode = inhMode
        inhCodes.append(inhCode)
    return "/".join(inhCodes)

# --- MAIN ---
def main():
    inFname, chromSizesFname, outFname = sys.argv[1:]

    tmpFname = "omimGene2.bed"
    ofh = open(tmpFname, "w")

    for line in open(inFname):
        row = line.rstrip("\n").split("\t")
        # 1  chr12
        # 2  4477392
        # 3  4488878
        # 4  605380
        # 5  0
        # 6  .
        # 7  4477392
        # 8  4488878
        # 9  color
        #10  FGF23, ADHR, HPDR2, PHPTC, HFTC2
        #11  <old disorder string from old OMIM pipeline>
        #12  Tumoral calcinosis, hyperphosphatemic, familial, 2|3|$Hypophosphatemic rickets, autosomal dominant|3|Autosomal dominant
        #13  PhenoMapKey
        chrom, start, end, mimId, score, strand, thickStart, thickEnd, dummyColor, syms, oldDisorderStr, phenoStr,phenoMapKey = row
        
        mapKeys = []
        if phenoStr=="":
            newPhenoStr = ""
            newPhenoPlEnding = ""
        else:
            phenoList = phenoStr.split("$")
            newPhenos = []

            for phenoPart in phenoList:
                name, mapKey, inhMode = phenoPart.split("|")
                inhCode = inhModeToCode(inhMode)
                phenoLabels = []
                phenoLabels.append(name)
                if inhMode!="":
                    phenoLabels.append(inhCode)
                if mapKey!="":
                    phenoLabels.append(mapKey)
                phenoLabel = ", ".join(phenoLabels)
                newPhenos.append(phenoLabel)

                if mapKey != "":
                    mapKeys.append(mapKey)
            newPhenoStr = "; ".join(newPhenos)
            if len(newPhenos)>1:
                newPhenoPlEnding = "s"

        mainSym = syms.split(", ")[0]
        altSyms = syms.split(", ")[1:]
        #chrom, start, end, name, score, strand, thickStart, thickEnd, color, syms, desc, phenoStr = row
        synPlEnding = ""
        if len(altSyms)>1:
            synPlEnding = "s"

        if len(altSyms)>0:
            mouseOver = "Gene: %s, Synonym%s: %s, Phenotype%s: %s" % \
                (mainSym, synPlEnding, ", ".join(altSyms), newPhenoPlEnding, newPhenoStr)
        elif len(newPhenoStr)==0:
            mouseOver = "Gene: %s" % (mainSym)
        else:
            mouseOver = "Gene: %s, Phenotype%s: %s" % (mainSym, newPhenoPlEnding, newPhenoStr)

        if len(mapKeys)==0:
            color = "190,190,190"
        else:
            mapKeys.sort()
            maxKey = mapKeys[-1]
            if maxKey=="4":
                color = "105,50,155"
            elif maxKey=="3":
                color = "0,85,0"
            elif maxKey=="2":
                color = "102,150,102"
            elif maxKey=="1":
                color = "170,196,170"
            else:
                assert(False)
            
        newRow = (chrom, start, end, mimId, score, strand, thickStart, thickEnd, color, mainSym, phenoMapKey, mouseOver)
        ofh.write("\t".join(newRow))
        ofh.write("\n")

    #ofh.flush()
    ofh.close()

    cmd = "bedSort %s %s" % (tmpFname, tmpFname)
    os.system(cmd)

    cmd = "bedToBigBed %s %s %s -tab -extraIndex=name -as=omimGene2.as -type=bed9+" % (tmpFname, chromSizesFname, outFname)
    os.system(cmd)

main()
