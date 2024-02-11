import subprocess

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

def splitFiles(pathToFile,dbs):
    originalFile = open(pathToFile,'r')
    outputFileHaplo = open("/hive/data/outside/collinsHaploTriploScores/pHaploDosageSensitivity"+dbs+".bed",'w')
    outputFileTriplo = open("/hive/data/outside/collinsHaploTriploScores/pTriploDosageSensitivity"+dbs+".bed",'w')
    
    for line in originalFile:
        line = line.rstrip()
        line = line.split("\t")
        pHaplo = round(float(line[10]),2)
        pTriplo = round(float(line[11]),2)
        #Color according to paper: https://europepmc.org/article/MED/35917817
        if pHaplo >= 0.86:
            pHaploItemColor = "181,2,14"
        else:
            pHaploItemColor = "250,42,27"
        if pTriplo >= 0.94:
            pTriploItemColor = "0,9,138"
        else:
            pTriploItemColor = "87,92,252"
        outputFileHaplo.write("\t".join(line[0:8])+"\t"+pHaploItemColor+"\t"+line[9]+"\t"+str(pHaplo)+"\n")
        outputFileTriplo.write("\t".join(line[0:8])+"\t"+pTriploItemColor+"\t"+line[9]+"\t"+str(pTriplo)+"\n")
    originalFile.close()
    outputFileHaplo.close()
    outputFileTriplo.close()
    
def fetchGeneInfo(geneSymbol,ensVersion,dbs):
    """Performs mysql search on GENCODE table and retrieves largest txStart/txEnd for entire gene"""
    buildGene = {}
    finalGeneOutput = {}
    buildGene['txStart'] = []
    buildGene['txEnd'] = [] 
    GENCODEoutput = bash("""hgsql -Ne "select name,chrom,strand,txStart,txEnd from wgEncodeGencodeComp"""+ensVersion+""" where name2='"""+geneSymbol+"""'" """+dbs)
    GENCODEoutput = GENCODEoutput.rstrip().split("\n")
    try:
        for geneEntry in GENCODEoutput:
            if "fix" not in str(geneEntry) and "alt" not in str(geneEntry) and "hap" not in str(geneEntry) and "_gl" not in str(geneEntry):
                geneEntry = geneEntry.split("\t")
                finalGeneOutput['ensTranscript'] = geneEntry[0]
                
                #Look for ENSTs across difference chromosomes, like HIST1H4J
                try:
                    if buildGene['chrom'] != geneEntry[1]:
                        print("Multi-chromosome gene: "+geneSymbol)
                except:
                    pass
                
                #Make special exception for HIST1H4J on hg38 that has transcripts on chr1 and chr6
                if geneSymbol == "HIST1H4J" and dbs == "hg38":
                    buildGene['chrom'] = "chr1"
                    buildGene['strand'] = "+"
                    buildGene['txStart'].append(int(149832658))
                    buildGene['txEnd'].append(int(149839767))
                else:
                    buildGene['chrom'] = geneEntry[1]
                    buildGene['strand'] = geneEntry[2]
                    buildGene['txStart'].append(int(geneEntry[3]))
                    buildGene['txEnd'].append(int(geneEntry[4]))
                    
        finalGeneOutput['chrom'] = buildGene['chrom']
        finalGeneOutput['strand'] = buildGene['strand']
        finalGeneOutput['txStart'] = str(min(buildGene['txStart']))
        finalGeneOutput['txEnd'] = str(max(buildGene['txEnd']))
        #try to get the ENSGid
        try:
            ensgFetch = bash("""hgsql -Ne "select geneId from wgEncodeGencodeAttrs"""+ensVersion+""" where transcriptId='"""+finalGeneOutput['ensTranscript']+"""'" """+dbs)
            finalGeneOutput['ensgID'] = ensgFetch.rstrip()
        except:
            print("Error with table. Look at wgEncodeGencodeAttrs"+ensVersion)
            
        return(finalGeneOutput)
    except:
        pass

def makeManeDic():
    """Iterates through latest mane file and builds dictionary out of gene symbols and values"""
    bash("bigBedToBed /gbdb/hg38/mane/mane.bb /hive/data/outside/collinsHaploTriploScores/mane.bed")
    openFile = open("/hive/data/outside/collinsHaploTriploScores/mane.bed",'r')
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
    bash("rm -f /hive/data/outside/collinsHaploTriploScores/mane.bed")
    return(maneGeneSymbolCoords)

def buildFileHg38(dosageScoresFile,outPutFile,missingGenesFromHg38):
    inputDosageScoresFile = open(dosageScoresFile,'r')
    outputHg38File = open(outPutFile,'w')
    missingGenesFromHg38 = open(missingGenesFromHg38, 'w')
    n=0
    problem = 0
    maneGeneSymbolCoords = makeManeDic()
    badItems = []
    missingGenes = []
    for line in inputDosageScoresFile:
        if n % 1000 == 0:
            print("Proccessing line "+str(n)+" of file.")
        n+=1
        if line.startswith('#'):
            continue
        else:
            line = line.rstrip()
            line = line.split("\t")
            if len(line) == 3:            
                geneSymbol = line[0]
                classificationRgb = "252,129,129"
                try:
                    chrom = maneGeneSymbolCoords[geneSymbol]['chrom']
                    chromStart = maneGeneSymbolCoords[geneSymbol]['chromStart']
                    chromEnd = maneGeneSymbolCoords[geneSymbol]['chromEnd']
                    strand = maneGeneSymbolCoords[geneSymbol]['strand']
                    ensGene = maneGeneSymbolCoords[geneSymbol]['ensGene']
                    outputHg38File.write("%s\t%s\t%s\t%s\t0\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,geneSymbol,
                        strand,chromStart,chromEnd,classificationRgb,ensGene,"\t".join(line[1:])))
                except:
                    try:
                        geneDic = fetchGeneInfo(geneSymbol,'V45','hg38')
                        chrom = geneDic['chrom']
                        chromStart = geneDic['txStart']
                        chromEnd = geneDic['txEnd']
                        strand = geneDic['strand']
                        ensGene = geneDic['ensgID']
                        outputHg38File.write("%s\t%s\t%s\t%s\t0\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,geneSymbol,
                            strand,chromStart,chromEnd,classificationRgb,ensGene,"\t".join(line[1:])))
                    except:
                        try:
                            geneDic = fetchGeneInfo(geneSymbol,'V20','hg38')
                            chrom = geneDic['chrom']
                            chromStart = geneDic['txStart']
                            chromEnd = geneDic['txEnd']
                            strand = geneDic['strand']
                            ensGene = geneDic['ensgID']
                            outputHg38File.write("%s\t%s\t%s\t%s\t0\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,geneSymbol,
                                strand,chromStart,chromEnd,classificationRgb,ensGene,"\t".join(line[1:])))
                
                        except:      
                            missingGenes.append(geneSymbol)
                            problem+=1
            else:
                badItems.append(line)

    inputDosageScoresFile.close()
    outputHg38File.close()
    if badItems != []:
        print(str(len(badItems))+" bad lines were found in the file, see below:\n\n")
        for item in badItems:
            print(item)
    for item in missingGenes:
        missingGenesFromHg38.write(item+"\n")
    missingGenesFromHg38.close()
    print("\n\nhg38 bed file completed. See file missingHg38GenesList.txt. Total number of failed entries: "+str(problem))
    print("\n\nhg38 bed file completed. Total number of failed entries: "+str(problem))    

def buildFileHg19(dosageScoresFile,outPutFile):
    inputDosageScoresFile = open(dosageScoresFile,'r')
    outputHg19File = open(outPutFile,'w')
    n=0
    problem=0
    badItems = []
    for line in inputDosageScoresFile:
        if n % 1000 == 0:
            print("Proccessing line "+str(n)+" of file.")
        n+=1
        if line.startswith('#'):
            continue
        else:
            line = line.rstrip()
            line = line.split("\t")
            if len(line) == 3:            
                geneSymbol = line[0]
                pHaplo = float(line[1])
                pTriplo = float(line[2])
                classificationRgb = "252,129,129"
                try:
                    geneDic = fetchGeneInfo(geneSymbol,'V19','hg19')
                    chrom = geneDic['chrom']
                    chromStart = geneDic['txStart']
                    chromEnd = geneDic['txEnd']
                    strand = geneDic['strand']
                    ensTranscript = geneDic['ensTranscript']
                    ensgID = geneDic['ensgID']
                    outputHg19File.write("%s\t%s\t%s\t%s\t0\t%s\t%s\t%s\t%s\t%s\t%s\n" % (chrom,chromStart,chromEnd,geneSymbol,
                            strand,chromStart,chromEnd,classificationRgb,ensgID,"\t".join(line[1:])))                
                except:
                    print("Problem entry.")
                    problem+=1
            else:
                badItems.append(line)

    inputDosageScoresFile.close()
    outputHg19File.close()
    if badItems != []:
        print(str(len(badItems))+" bad lines were found in the file, see below:\n\n")
        for item in badItems:
            print(item)
    if problem != 0:
        print("\n\nhg19 genCC bed file completed. Total number of mismapped genes:: "+str(problem))

#Start building track
workDir = "/hive/data/outside/collinsHaploTriploScores"
bash("mkdir -p "+workDir)
hg19outPutFile = workDir+"/hg19dosageScores.bed"
hg38outPutFile = workDir+"/hg38dosageScores.bed"
missingGenesFromHg38 = workDir+"/missingHg38GenesList.txt"
dosageScoresTsvFile = "/hive/data/outside/collinsHaploTriploScores/rCNV.gene_scores.tsv"

buildFileHg19(dosageScoresTsvFile,hg19outPutFile)
buildFileHg38(dosageScoresTsvFile,hg38outPutFile,missingGenesFromHg38)

#build file of missing genes and liftOver the missing hg38 genes from hg19
bash("cat "+hg19outPutFile+" | grep -w -f /hive/data/outside/collinsHaploTriploScores/missingHg38GenesList.txt > /hive/data/outside/collinsHaploTriploScores/finalMissingHg38Genes.bed")
bash("liftOver -bedPlus=9 -tab /hive/data/outside/collinsHaploTriploScores/finalMissingHg38Genes.bed /hive/data/outside/collinsHaploTriploScores/hg19ToHg38.over.chain /hive/data/outside/collinsHaploTriploScores/hg38missingGenes.bed /hive/data/outside/collinsHaploTriploScores/unmapped.bed")
finalGeneCountNotMappedToHg38 = bash("wc -l /hive/data/outside/collinsHaploTriploScores/unmapped.bed")
print("The total number of files that were not able to be mapped by any means to hg38 were "+str(int(finalGeneCountNotMappedToHg38.split(" ")[0])/2))
print("Most of these regions were split in hg38, see unmapped file for details: /hive/data/outside/collinsHaploTriploScores/unmapped.bed")

#Add the lifted entries to the hg38 file
hg38outPutFile = workDir+"/hg38dosageScoresFinal.bed"
bash("cat /hive/data/outside/collinsHaploTriploScores/hg38dosageScores.bed > "+hg38outPutFile)
bash("cat /hive/data/outside/collinsHaploTriploScores/hg38missingGenes.bed >> "+hg38outPutFile)

#Sort the files
bash("bedSort "+hg38outPutFile+" "+hg38outPutFile)
bash("bedSort "+hg19outPutFile+" "+hg19outPutFile)

#Split the files into two separate tracks for each score and add color for items
splitFiles(hg38outPutFile,"Hg38")
splitFiles(hg19outPutFile,"Hg19")

#Create hg38 bb file and symlink
hg38pHaploFinalFile = "/hive/data/outside/collinsHaploTriploScores/pHaploDosageSensitivityHg38.bed"
hg38pTriploFinalFile = "/hive/data/outside/collinsHaploTriploScores/pTriploDosageSensitivityHg38.bed"
bash("bedToBigBed -as=/hive/data/outside/collinsHaploTriploScores/dosageSensitivityHaplo.as -extraIndex=name -type=bed9+2 -tab "+hg38pHaploFinalFile+" /cluster/data/hg38/chrom.sizes "+hg38pHaploFinalFile.split(".")[0]+".bb")
bash("bedToBigBed -as=/hive/data/outside/collinsHaploTriploScores/dosageSensitivityTriplo.as -extraIndex=name -type=bed9+2 -tab "+hg38pTriploFinalFile+" /cluster/data/hg38/chrom.sizes "+hg38pTriploFinalFile.split(".")[0]+".bb")

bash("ln -s "+hg38pHaploFinalFile.split(".")[0]+".bb /gbdb/hg38/bbi/dosageSensitivityCollins2022/pHaploDosageSensitivity.bb")
bash("ln -s "+hg38pTriploFinalFile.split(".")[0]+".bb /gbdb/hg38/bbi/dosageSensitivityCollins2022/pTriploDosageSensitivity.bb")

#Create hg19 bb file and symlink
hg19pHaploFinalFile = "/hive/data/outside/collinsHaploTriploScores/pHaploDosageSensitivityHg19.bed"
hg19pTriploFinalFile = "/hive/data/outside/collinsHaploTriploScores/pTriploDosageSensitivityHg19.bed"
bash("bedToBigBed -as=/hive/data/outside/collinsHaploTriploScores/dosageSensitivityHaplo.as -extraIndex=name -type=bed9+2 -tab "+hg19pHaploFinalFile+" /cluster/data/hg19/chrom.sizes "+hg19pHaploFinalFile.split(".")[0]+".bb")
bash("bedToBigBed -as=/hive/data/outside/collinsHaploTriploScores/dosageSensitivityTriplo.as -extraIndex=name -type=bed9+2 -tab "+hg19pTriploFinalFile+" /cluster/data/hg19/chrom.sizes "+hg19pTriploFinalFile.split(".")[0]+".bb")

bash("ln -s "+hg19pHaploFinalFile.split(".")[0]+".bb /gbdb/hg19/bbi/dosageSensitivityCollins2022/pHaploDosageSensitivity.bb")
bash("ln -s "+hg19pTriploFinalFile.split(".")[0]+".bb /gbdb/hg19/bbi/dosageSensitivityCollins2022/pTriploDosageSensitivity.bb")

print("DosageSensitivity track complete.")
