#!/usr/bin/env python3
#Made otto by Lou 9/14/2023

import subprocess
import csv
import re
import sys
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

def getLatestGencodeTable(db, pattern):
    """Find the latest GENCODE table matching a pattern like 'wgEncodeGencodeComp%' or
    'wgEncodeGencodeComp%lift37'. Returns the table name with the highest version number."""
    output = bash('hgsql -Ne "show tables like \''+pattern+'\'" '+db)
    tables = output.rstrip().split("\n")
    tables = [t for t in tables if t]  # filter empty strings
    if not tables:
        raise RuntimeError("No GENCODE tables found matching '"+pattern+"' in "+db)
    # Extract version number from each table name and find the max
    best = None
    bestVersion = -1
    for t in tables:
        match = re.search(r'V(\d+)', t)
        if match:
            version = int(match.group(1))
            if version > bestVersion:
                bestVersion = version
                best = t
    if best is None:
        raise RuntimeError("Could not parse version from GENCODE tables in "+db)
    print("Using GENCODE table: "+best+" (version "+str(bestVersion)+") for "+db)
    return(best)

def fetchGeneInfo(geneSymbol, table, dbs):
    """Performs mysql search on GENCODE table and retrieves largest txStart/txEnd for entire gene.
    Groups results by chromosome and picks the chromosome with the most transcripts to avoid
    mixing coordinates from multi-copy gene families."""
    GENCODEoutput = bash("""hgsql -Ne "select name,chrom,strand,txStart,txEnd from """+table+""" where name2='"""+geneSymbol+"""'" """+dbs)
    GENCODEoutput = GENCODEoutput.rstrip().split("\n")
    try:
        # Group transcripts by chromosome, filtering out fix/alt/hap
        chromGroups = {}
        for geneEntry in GENCODEoutput:
            if "fix" not in str(geneEntry) and "alt" not in str(geneEntry) and "hap" not in str(geneEntry):
                fields = geneEntry.split("\t")
                chrom = fields[1]
                if chrom not in chromGroups:
                    chromGroups[chrom] = []
                chromGroups[chrom].append(fields)
        if not chromGroups:
            print("Following gene symbol was not found: "+geneSymbol)
            return(None)
        # Pick the chromosome with the most transcripts
        bestChrom = max(chromGroups, key=lambda c: len(chromGroups[c]))
        transcripts = chromGroups[bestChrom]
        finalGeneOutput = {}
        if len(transcripts) == 1:
            finalGeneOutput['ensTranscript'] = transcripts[0][0]
        else:
            finalGeneOutput['ensTranscript'] = ""
        finalGeneOutput['chrom'] = bestChrom
        finalGeneOutput['strand'] = transcripts[0][2]
        finalGeneOutput['txStart'] = str(min(int(t[3]) for t in transcripts))
        finalGeneOutput['txEnd'] = str(max(int(t[4]) for t in transcripts))
        return(finalGeneOutput)
    except Exception as e:
        print("Following gene symbol was not found: "+geneSymbol+" ("+str(e)+")")
        return(None)

def fetchGeneInfoHg19(nmAccession, table, dbs):
    """Performs mysql search on ncbiRefSeq table and retrieves largest txStart/txEnd for entire gene.
    Groups results by chromosome and picks the chromosome with the most transcripts."""
    GENCODEoutput = bash("""hgsql -Ne "select name,chrom,strand,txStart,txEnd from """+table+""" where name='"""+nmAccession+"""'" """+dbs)
    GENCODEoutput = GENCODEoutput.rstrip().split("\n")
    try:
        # Group transcripts by chromosome, filtering out fix/alt/hap
        chromGroups = {}
        for geneEntry in GENCODEoutput:
            if "fix" not in str(geneEntry) and "alt" not in str(geneEntry) and "hap" not in str(geneEntry):
                fields = geneEntry.split("\t")
                chrom = fields[1]
                if chrom not in chromGroups:
                    chromGroups[chrom] = []
                chromGroups[chrom].append(fields)
        if not chromGroups:
            print("Following refSeq accession was not found: "+nmAccession)
            return(None)
        # Pick the chromosome with the most transcripts
        bestChrom = max(chromGroups, key=lambda c: len(chromGroups[c]))
        transcripts = chromGroups[bestChrom]
        finalGeneOutput = {}
        if len(transcripts) == 1:
            finalGeneOutput['ensTranscript'] = transcripts[0][0]
        else:
            finalGeneOutput['ensTranscript'] = ""
        finalGeneOutput['chrom'] = bestChrom
        finalGeneOutput['strand'] = transcripts[0][2]
        finalGeneOutput['txStart'] = str(min(int(t[3]) for t in transcripts))
        finalGeneOutput['txEnd'] = str(max(int(t[4]) for t in transcripts))
        return(finalGeneOutput)
    except Exception as e:
        print("Following refSeq accession was not found: "+nmAccession+" ("+str(e)+")")
        return(None)

def fetchGeneInfoKnownGene(geneSymbol, dbs):
    """Fallback lookup using knownGene + kgXref join. Catches genes missing from GENCODE
    Comprehensive (pseudogenes, recently added genes, etc.)."""
    output = bash("""hgsql -Ne "select kg.name,kg.chrom,kg.strand,kg.txStart,kg.txEnd from knownGene kg, kgXref kx where kg.name=kx.kgID and kx.geneSymbol='"""+geneSymbol+"""'" """+dbs)
    output = output.rstrip().split("\n")
    try:
        chromGroups = {}
        for entry in output:
            if "fix" not in str(entry) and "alt" not in str(entry) and "hap" not in str(entry):
                fields = entry.split("\t")
                chrom = fields[1]
                if chrom not in chromGroups:
                    chromGroups[chrom] = []
                chromGroups[chrom].append(fields)
        if not chromGroups:
            return(None)
        bestChrom = max(chromGroups, key=lambda c: len(chromGroups[c]))
        transcripts = chromGroups[bestChrom]
        finalGeneOutput = {}
        if len(transcripts) == 1:
            finalGeneOutput['ensTranscript'] = transcripts[0][0]
        else:
            finalGeneOutput['ensTranscript'] = ""
        finalGeneOutput['chrom'] = bestChrom
        finalGeneOutput['strand'] = transcripts[0][2]
        finalGeneOutput['txStart'] = str(min(int(t[3]) for t in transcripts))
        finalGeneOutput['txEnd'] = str(max(int(t[4]) for t in transcripts))
        return(finalGeneOutput)
    except Exception:
        return(None)

def makeManeDic():
    """Iterates through latest mane file and builds dictionary out of gene symbols and values"""
    bash("bigBedToBed /gbdb/hg38/mane/mane.bb /hive/data/outside/otto/genCC/mane.bed")
    maneGeneSymbolCoords = {}
    with open("/hive/data/outside/otto/genCC/mane.bed",'r') as openFile:
        for line in openFile:
            line = line.rstrip()
            line = line.split("\t")
            geneSymbol = line[18]
            if geneSymbol not in maneGeneSymbolCoords:
                maneGeneSymbolCoords[geneSymbol] = {}
                maneGeneSymbolCoords[geneSymbol]['chrom'] = line[0]
                maneGeneSymbolCoords[geneSymbol]['chromStart'] = line[1]
                maneGeneSymbolCoords[geneSymbol]['chromEnd'] = line[2]
                maneGeneSymbolCoords[geneSymbol]['strand'] = line[5]
                maneGeneSymbolCoords[geneSymbol]['ensGene'] = line[17]
                maneGeneSymbolCoords[geneSymbol]['ensTranscript'] = line[3]
                maneGeneSymbolCoords[geneSymbol]['refSeqAccession'] = line[21]
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
        classificationRgb = "0,0,0"
    return(classificationRgb)

def writeHg38Line(outputFile, chrom, chromStart, chromEnd, genCCname, strand,
                  classificationRgb, ensTranscript, ensGene, refSeqAccession, line):
    """Write a single hg38 bed line, validating coordinates first."""
    if int(chromStart) > int(chromEnd):
        print("Skipping record with inverted coordinates: "+genCCname+" "+chrom+":"+chromStart+"-"+chromEnd)
        return(False)
    # Sanitize fields: replace embedded newlines and tabs with spaces for bed format compatibility
    sanitized = [field.replace("\n", " ").replace("\r", " ").replace("\t", " ") for field in line]
    outputFile.write("%s\t%s\t%s\t%s\t0\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,genCCname,
        strand,chromStart,chromEnd,classificationRgb,ensTranscript,ensGene,refSeqAccession,"\t".join(sanitized)))
    return(True)

def buildFileHg38(genCCfile, outPutFile, gencodeTable):
    n=0
    maneGeneSymbolCoords = makeManeDic()
    with open(genCCfile, newline='', encoding="utf-8") as inputGenCCFile, \
         open(outPutFile, 'w', encoding='utf-8') as outputHg38File:
        reader = csv.reader(inputGenCCFile, delimiter='\t', quotechar='"')
        header = next(reader)
        for line in reader:
            if len(line) != 31:
                print("Skipping malformed row with "+str(len(line))+" fields: "+str(line[:3]))
                continue
            geneSymbol = line[3]
            genCCname = line[3]+" "+line[6]
            classificationRgb = assignRGBcolorByEvidenceClassification(line[9])
            try:
                chrom = maneGeneSymbolCoords[geneSymbol]['chrom']
                chromStart = maneGeneSymbolCoords[geneSymbol]['chromStart']
                chromEnd = maneGeneSymbolCoords[geneSymbol]['chromEnd']
                strand = maneGeneSymbolCoords[geneSymbol]['strand']
                ensGene = maneGeneSymbolCoords[geneSymbol]['ensGene']
                ensTranscript = maneGeneSymbolCoords[geneSymbol]['ensTranscript']
                refSeqAccession = maneGeneSymbolCoords[geneSymbol]['refSeqAccession']
                writeHg38Line(outputHg38File, chrom, chromStart, chromEnd, genCCname,
                    strand, classificationRgb, ensTranscript, ensGene, refSeqAccession, line)
            except Exception:
                # Fallback 1: GENCODE Comprehensive
                geneDic = fetchGeneInfo(geneSymbol, gencodeTable, 'hg38')
                if geneDic is not None:
                    wrote = writeHg38Line(outputHg38File, geneDic['chrom'], geneDic['txStart'],
                        geneDic['txEnd'], genCCname, geneDic['strand'], classificationRgb,
                        geneDic['ensTranscript'], "", "", line)
                    if wrote:
                        continue
                # Fallback 2: knownGene + kgXref
                geneDic = fetchGeneInfoKnownGene(geneSymbol, 'hg38')
                if geneDic is not None:
                    wrote = writeHg38Line(outputHg38File, geneDic['chrom'], geneDic['txStart'],
                        geneDic['txEnd'], genCCname, geneDic['strand'], classificationRgb,
                        geneDic['ensTranscript'], "", "", line)
                    if wrote:
                        continue
                n+=1
                print("No hg38 match for: "+geneSymbol)

    print("\nhg38 genCC bed file completed. Total number of failed entries: "+str(n))

def buildFileHg19(genCCfile, outPutFile, gencodeTable):
    n=0
    skippedCoords=0
    with open(genCCfile, 'r', encoding="utf-8") as hg38GenCCbedFile, \
         open(outPutFile, 'w', encoding='utf-8') as outputHg19File:
        for line in hg38GenCCbedFile:
            line = line.rstrip()
            line = line.split("\t")
            geneSymbol = line[15]
            nmAccession = line[11]
            geneDic = None
            if nmAccession != "":
                geneDic = fetchGeneInfoHg19(nmAccession, 'ncbiRefSeq', 'hg19')
            if geneDic is None:
                geneDic = fetchGeneInfo(geneSymbol, gencodeTable, 'hg19')
            if geneDic is None:
                geneDic = fetchGeneInfoKnownGene(geneSymbol, 'hg19')
            if geneDic is None:
                n+=1
                print("No hg19 match for: "+geneSymbol)
                continue
            chrom = geneDic['chrom']
            chromStart = geneDic['txStart']
            chromEnd = geneDic['txEnd']
            # Validate coordinates before writing
            if int(chromStart) > int(chromEnd):
                skippedCoords+=1
                print("Skipping hg19 record with inverted coordinates: "+geneSymbol+" "+chrom+":"+chromStart+"-"+chromEnd)
                continue
            outputHg19File.write("%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,
                                    "\t".join(line[3:6]),chromStart,chromEnd,"\t".join(line[8:])))

    print("hg19 genCC bed file completed. Total number of failed entries: "+str(n))
    if skippedCoords > 0:
        print("Warning: "+str(skippedCoords)+" hg19 entries skipped due to inverted coordinates")

def checkIfUpdateIsNeeded():
    bash("wget -q 'https://thegencc.org/download/action/submissions-export-tsv?format=new' -O /hive/data/outside/otto/genCC/newSubmission.tsv")
    newMd5sum = bash("md5sum /hive/data/outside/otto/genCC/newSubmission.tsv")
    newMd5sum = newMd5sum.split("  ")[0]
    oldMd5sum = bash("md5sum /hive/data/outside/otto/genCC/prevSubmission.tsv")
    oldMd5sum = oldMd5sum.split("  ")[0]
    if oldMd5sum != newMd5sum:
        return(True)
    else:
        return(False)

def checkItemCounts(oldBb, newBb):
    """Compare item counts between old and new bigBed files. Exit if difference > 10%."""
    oldItemCount = bash('bigBedInfo '+oldBb+' | grep "itemCount"')
    oldItemCount = int(oldItemCount.rstrip().split("itemCount: ")[1].replace(",",""))
    newItemCount = bash('bigBedInfo '+newBb+' | grep "itemCount"')
    newItemCount = int(newItemCount.rstrip().split("itemCount: ")[1].replace(",",""))
    if abs(newItemCount - oldItemCount) > 0.1 * max(newItemCount, oldItemCount):
        sys.exit("Item count difference >10% for "+newBb+": old="+str(oldItemCount)+" new="+str(newItemCount))
    print(oldBb+" old: "+str(oldItemCount)+" new: "+str(newItemCount))

if checkIfUpdateIsNeeded():
    date = str(datetime.now()).split(" ")[0]
    workDir = "/hive/data/outside/otto/genCC/"+date
    bash("mkdir -p "+workDir)
    hg19outPutFile = workDir+"/hg19genCC.bed"
    hg38outPutFile = workDir+"/hg38genCC.bed"
    bash("cp /hive/data/outside/otto/genCC/newSubmission.tsv "+workDir)
    genCCtsvFile = "/hive/data/outside/otto/genCC/newSubmission.tsv"

    # Auto-detect latest GENCODE Comprehensive tables
    gencodeHg38 = getLatestGencodeTable('hg38', 'wgEncodeGencodeComp%')
    gencodeHg19 = getLatestGencodeTable('hg19', 'wgEncodeGencodeComp%lift37')

    buildFileHg38(genCCtsvFile, hg38outPutFile, gencodeHg38)
    buildFileHg19(hg38outPutFile, hg19outPutFile, gencodeHg19)
    bash("mv /hive/data/outside/otto/genCC/newSubmission.tsv /hive/data/outside/otto/genCC/prevSubmission.tsv")

    bash("bedSort "+hg38outPutFile+" "+hg38outPutFile)
    bash("bedToBigBed -as=/hive/data/genomes/hg38/bed/genCC/genCC.as -sort -extraIndex=gene_symbol -type=bed9+34 -tab "+hg38outPutFile+" /cluster/data/hg38/chrom.sizes "+hg38outPutFile.split(".")[0]+".bb")
    print("Final file created: "+hg38outPutFile.split(".")[0]+".bb")

    bash("bedSort "+hg19outPutFile+" "+hg19outPutFile)
    bash("bedToBigBed -as=/hive/data/genomes/hg38/bed/genCC/genCC.as -sort -extraIndex=gene_symbol -type=bed9+34 -tab "+hg19outPutFile+" /cluster/data/hg19/chrom.sizes "+hg19outPutFile.split(".")[0]+".bb")
    print("Final file created: "+hg19outPutFile.split(".")[0]+".bb")

    checkItemCounts("/gbdb/hg38/bbi/genCC.bb", hg38outPutFile.split(".")[0]+".bb")
    checkItemCounts("/gbdb/hg19/bbi/genCC.bb", hg19outPutFile.split(".")[0]+".bb")

    bash("rm -f /gbdb/hg38/bbi/genCC.bb")
    bash("rm -f /gbdb/hg19/bbi/genCC.bb")

    bash("ln -s "+hg38outPutFile.split(".")[0]+".bb /gbdb/hg38/bbi/genCC.bb")
    bash("ln -s "+hg19outPutFile.split(".")[0]+".bb /gbdb/hg19/bbi/genCC.bb")

    print("GenCC updated.")
