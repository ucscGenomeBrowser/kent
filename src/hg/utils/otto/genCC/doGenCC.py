#!/usr/bin/env python3
#Made otto by Lou 9/14/2023

import subprocess
from datetime import datetime

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

def fetchGeneInfo(geneSymbol,table,dbs):
    """Performs mysql search on GENCODE table and retrieves largest txStart/txEnd for entire gene"""
    buildGene = {}
    finalGeneOutput = {}
    buildGene['txStart'] = []
    buildGene['txEnd'] = []
    GENCODEoutput = bash("""hgsql -Ne "select name,chrom,strand,txStart,txEnd from """+table+""" where name2='"""+geneSymbol+"""'" """+dbs)
    GENCODEoutput = GENCODEoutput.rstrip().split("\n")
    try:
        for geneEntry in GENCODEoutput:
            if "fix" not in str(geneEntry) and "alt" not in str(geneEntry) and "hap" not in str(geneEntry):
                geneEntry = geneEntry.split("\t")
                if len(GENCODEoutput) == 1:
                    finalGeneOutput['ensTranscript'] = geneEntry[0]
                else:
                    finalGeneOutput['ensTranscript'] = ""
                buildGene['chrom'] = geneEntry[1]
                buildGene['strand'] = geneEntry[2]
                buildGene['txStart'].append(geneEntry[3])
                buildGene['txEnd'].append(geneEntry[4])
        finalGeneOutput['chrom'] = buildGene['chrom']
        finalGeneOutput['strand'] = buildGene['strand']
        finalGeneOutput['txStart'] = min(buildGene['txStart'])
        finalGeneOutput['txEnd'] = max(buildGene['txEnd'])
        return(finalGeneOutput)
    except:
        print("Following gene symbol was not found:"+ geneSymbol)

def fetchGeneInfoHg19(nmAccession,table,dbs):
    """Performs mysql search on ncbiRefSeq table and retrieves largest txStart/txEnd for entire gene"""
    buildGene = {}
    finalGeneOutput = {}
    buildGene['txStart'] = []
    buildGene['txEnd'] = []
    GENCODEoutput = bash("""hgsql -Ne "select name,chrom,strand,txStart,txEnd from """+table+""" where name='"""+nmAccession+"""'" """+dbs)
    GENCODEoutput = GENCODEoutput.rstrip().split("\n")
    try:
        for geneEntry in GENCODEoutput:
            if "fix" not in str(geneEntry) and "alt" not in str(geneEntry) and "hap" not in str(geneEntry):
                geneEntry = geneEntry.split("\t")
                if len(GENCODEoutput) == 1:
                    finalGeneOutput['ensTranscript'] = geneEntry[0]
                else:
                    finalGeneOutput['ensTranscript'] = ""
                buildGene['chrom'] = geneEntry[1]
                buildGene['strand'] = geneEntry[2]
                buildGene['txStart'].append(geneEntry[3])
                buildGene['txEnd'].append(geneEntry[4])
        finalGeneOutput['chrom'] = buildGene['chrom']
        finalGeneOutput['strand'] = buildGene['strand']
        finalGeneOutput['txStart'] = min(buildGene['txStart'])
        finalGeneOutput['txEnd'] = max(buildGene['txEnd'])
        return(finalGeneOutput)
    except:
        print("Following refSeq accession was not found:"+ nmAccession)

def makeManeDic():
    """Iterates through latest mane  file and builds dictionary out of gene symbols and values"""
    bash("bigBedToBed /gbdb/hg38/mane/mane.bb /hive/data/outside/otto/genCC/mane.bed")
    openFile = open("/hive/data/outside/otto/genCC/mane.bed",'r')
    maneGeneSymbolCoords = {}
    for line in openFile:
        line = line.rstrip()
        line = line.split("\t")
        geneSymbol = line[18]
        if geneSymbol not in maneGeneSymbolCoords.keys():
            maneGeneSymbolCoords[geneSymbol] = {}
            maneGeneSymbolCoords[geneSymbol]['chrom'] = line[0]
            maneGeneSymbolCoords[geneSymbol]['chromStart'] = line[1]
            maneGeneSymbolCoords[geneSymbol]['chromEnd'] = line[2]
            maneGeneSymbolCoords[geneSymbol]['strand'] = line[5]
            maneGeneSymbolCoords[geneSymbol]['ensGene'] = line[17]
            maneGeneSymbolCoords[geneSymbol]['ensTranscript'] = line[3]
            maneGeneSymbolCoords[geneSymbol]['refSeqAccession'] = line[21]
    openFile.close()
    bash("rm -f /hive/data/outside/otto/genCC/mane.bed")
    return(maneGeneSymbolCoords)

def assignRGBcolorByEvidenceClassification(classification):
    if classification == "Definitive":
        classificationRgb = "56,161,105"
    elif classification == "Disputed Evidence":
        classificationRgb = "229,62,62"
    elif classification == "Limited":
        classificationRgb = "252,129,129"
    elif classification == "Moderate":
        classificationRgb = "104,211,145"
    elif classification == "No Known Disease Relationship":
        classificationRgb = "113,128,150"
    elif classification == "Refuted Evidence":
        classificationRgb = "155,44,44"
    elif classification == "Strong":
        classificationRgb = "39,103,73"
    elif classification == "Supportive":
        classificationRgb = "99,179,237"
    else:
        print("There was an unknown classification: "+classification)
    return(classificationRgb)

def buildFileHg38(genCCfile,outPutFile):
    inputGenCCFile = open(genCCfile,'r',encoding="utf-8")
    outputHg38File = open(outPutFile,'w',encoding='utf-8')
    n=0
    maneGeneSymbolCoords = makeManeDic()
    badItems = []
    for line in inputGenCCFile:
        if line.startswith('"uuid'):
            continue
        else:
            line = line.rstrip()
            line = line.split("\t")
            if len(line) == 30:            
                for each in range(len(line)):
                    line[each] = line[each][1:len(line[each])-1]
                geneSymbol = line[2]
#             genCCname = line[0] #Disease = 4 GENCC = 7 evidence = 8
                genCCname = line[2]+" "+line[5]
                classificationRgb = assignRGBcolorByEvidenceClassification(line[8])
                try:
                    chrom = maneGeneSymbolCoords[geneSymbol]['chrom']
                    chromStart = maneGeneSymbolCoords[geneSymbol]['chromStart']
                    chromEnd = maneGeneSymbolCoords[geneSymbol]['chromEnd']
                    strand = maneGeneSymbolCoords[geneSymbol]['strand']
                    ensGene = maneGeneSymbolCoords[geneSymbol]['ensGene']
                    ensTranscript = maneGeneSymbolCoords[geneSymbol]['ensTranscript']
                    refSeqAccession = maneGeneSymbolCoords[geneSymbol]['refSeqAccession']
                    outputHg38File.write("%s\t%s\t%s\t%s\t0\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,genCCname,
                        strand,chromStart,chromEnd,classificationRgb,ensTranscript,ensGene,refSeqAccession,"\t".join(line)))
                except:
                    try:
                        geneDic = fetchGeneInfo(geneSymbol,'wgEncodeGencodeCompV40','hg38')
                        chrom = geneDic['chrom']
                        chromStart = geneDic['txStart']
                        chromEnd = geneDic['txEnd']
                        strand = geneDic['strand']
                        ensGene = ""
                        ensTranscript = geneDic['ensTranscript']
                        refSeqAccession = ""
                        outputHg38File.write("%s\t%s\t%s\t%s\t0\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,genCCname,
                            strand,chromStart,chromEnd,classificationRgb,ensTranscript,ensGene,refSeqAccession,"\t".join(line)))
                    except:
                        n+=1
            else:
                badItems.append(line)

    inputGenCCFile.close()
    outputHg38File.close()
    print(str(len(badItems))+" lines were skipped in the file because they have incorrect formatting. This is usually because they had newline and tab characters in the info column. Usually fewer than 60 items are skipped this way. If the number is much greater, verify the script and update the estimate. The lines are printed below.\n\n")
    print("Note: This part of the code was commented out to reduce spam. Uncomment to see the lines.")
    #for item in badItems:
    #    print("\t".join(item).encode('utf-8'))
    print("\n\nhg38 genCC bed file completed. Total number of failed entries: "+str(n))

def buildFileHg19(genCCfile,outPutFile):
    hg38GenCCbedFile = open(genCCfile,'r',encoding="utf-8")
    outputHg19File = open(outPutFile,'w',encoding='utf-8')
    n=0
    for line in hg38GenCCbedFile:
        line = line.rstrip()
        line = line.split("\t")
        geneSymbol = line[14]
        nmAccession = line[11]
        if nmAccession != "":
            try:
                geneDic = fetchGeneInfoHg19(nmAccession,'ncbiRefSeq','hg19')
                chrom = geneDic['chrom']
                chromStart = geneDic['txStart']
                chromEnd = geneDic['txEnd']
                strand = geneDic['strand']
                outputHg19File.write("%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,
                                        "\t".join(line[3:6]),chromStart,chromEnd,"\t".join(line[8:])))

            except:
                try:
                    geneDic = fetchGeneInfo(geneSymbol,'wgEncodeGencodeCompV40lift37','hg19')
                    chrom = geneDic['chrom']
                    chromStart = geneDic['txStart']
                    chromEnd = geneDic['txEnd']
                    strand = geneDic['strand']
                    outputHg19File.write("%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,
                                        "\t".join(line[3:6]),chromStart,chromEnd,"\t".join(line[8:])))

                except:
                    n+=1
                    print("No match for: "+geneSymbol)
        else:
            try:
                geneDic = fetchGeneInfo(geneSymbol,'wgEncodeGencodeCompV40lift37','hg19')
                chrom = geneDic['chrom']
                chromStart = geneDic['txStart']
                chromEnd = geneDic['txEnd']
                strand = geneDic['strand']
                outputHg19File.write("%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,
                                        "\t".join(line[3:6]),chromStart,chromEnd,"\t".join(line[8:])))
            except:
                n+=1
                print("No match for: "+geneSymbol)
    hg38GenCCbedFile.close()
    outputHg19File.close()
    print("hg19 genCC bed file completed. Total number of failed entries: "+str(n))

def checkIfUpdateIsNeeded():
    bash("wget -q https://search.thegencc.org/download/action/submissions-export-tsv -O /hive/data/outside/otto/genCC/newSubmission.tsv")
    newMd5sum = bash("md5sum /hive/data/outside/otto/genCC/newSubmission.tsv")
    newMd5sum = newMd5sum.split("  ")[0]
    oldMd5sum = bash("md5sum /hive/data/outside/otto/genCC/prevSubmission.tsv")
    oldMd5sum = oldMd5sum.split("  ")[0]
    if oldMd5sum != newMd5sum:
        return(True)
    else:
        return(False)
    
if checkIfUpdateIsNeeded():
    date = str(datetime.now()).split(" ")[0]
    workDir = "/hive/data/outside/otto/genCC/"+date
    bash("mkdir -p "+workDir)
    hg19outPutFile = workDir+"/hg19genCC.bed"
    hg38outPutFile = workDir+"/hg38genCC.bed"
    bash("cp /hive/data/outside/otto/genCC/newSubmission.tsv "+workDir)
    genCCtsvFile = "/hive/data/outside/otto/genCC/newSubmission.tsv"
         
    buildFileHg38(genCCtsvFile,hg38outPutFile)
    buildFileHg19(hg38outPutFile,hg19outPutFile)
    bash("mv /hive/data/outside/otto/genCC/newSubmission.tsv /hive/data/outside/otto/genCC/prevSubmission.tsv")

    bash("bedSort "+hg38outPutFile+" "+hg38outPutFile)
    bash("bedToBigBed -as=/hive/data/genomes/hg38/bed/genCC/genCC.as -extraIndex=gene_symbol -type=bed9+33 -tab "+hg38outPutFile+" /cluster/data/hg38/chrom.sizes "+hg38outPutFile.split(".")[0]+".bb")
    print("Final file created: "+hg38outPutFile.split(".")[0]+".bb")

    bash("bedSort "+hg19outPutFile+" "+hg19outPutFile)
    bash("bedToBigBed -as=/hive/data/genomes/hg38/bed/genCC/genCC.as -extraIndex=gene_symbol -type=bed9+33 -tab "+hg19outPutFile+" /cluster/data/hg19/chrom.sizes "+hg19outPutFile.split(".")[0]+".bb")
    print("Final file created: "+hg19outPutFile.split(".")[0]+".bb")
         
    bash("rm -f /gbdb/hg38/bbi/genCC.bb")
    bash("rm -f /gbdb/hg19/bbi/genCC.bb")
         
    bash("ln -s "+hg38outPutFile.split(".")[0]+".bb /gbdb/hg38/bbi/genCC.bb")
    bash("ln -s "+hg19outPutFile.split(".")[0]+".bb /gbdb/hg19/bbi/genCC.bb")
         
    print("GenCC updated.")
