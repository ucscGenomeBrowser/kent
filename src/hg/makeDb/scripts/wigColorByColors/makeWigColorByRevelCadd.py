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

def assignScoreValue(score,trackName):
    if trackName == "revel":
        if score >= .644:
            prediction = "pathogenic"
            color = "255,0,0"
        elif score <= .290:
            prediction = "benign"
            color = "80,166,230"
        else:
            prediction = "neutral"
            color = "192,192,192"
    elif trackName == "cadd1.7":
        if score >= 25.3:
            prediction = "pathogenic"
            color = "255,0,0"
        elif score <= 22.7:
            prediction = "benign"
            color = "80,166,230"
        else:
            prediction = "neutral"
            color = "192,192,192"
    elif trackName == "alphaMissense":
        if score >= 0.564:
            prediction = "pathogenic"
            color = "255,0,0"
        elif score <= 0.34:
            prediction = "benign"
            color = "80,166,230"
        else:
            prediction = "neutral"
            color = "192,192,192"
    return(prediction,color)

def convertBigWigToBedGraph(fullBigWigFilePath,workDir,fileName):
    bash("bigWigToBedGraph "+fullBigWigFilePath+" "+workDir+fileName)

def createColorFileFromBigWig(workDir,fileName,trackName):
    bedFileName = fileName+".bed"
    inputFile = open(workDir+fileName,'r')
    outputFile = open(workDir+bedFileName,'w')
    n=0
    for line in inputFile:
        n+=1
        line = line.rstrip().split("\t")
        if n == 1:
            prevChrom = line[0]
            prevChromStart = line[1]
            prevChromEnd = line[2]
            prevPred,prevColor = assignScoreValue(float(line[3]),trackName)
        else:
            chrom = line[0]
            chromStart = line[1]
            chromEnd = line[2]
            pred,color = assignScoreValue(float(line[3]),trackName)

            #Check that the items are continuous, and then check if they'd have the same prediction to aggregate. Also not the end of a chrom
            if prevChromEnd == chromStart and prevPred == pred and chrom == prevChrom:
                prevChromEnd = chromEnd #Continuation, run the item and keep other values
            else:
                outputFile.write("\t".join([prevChrom,prevChromStart,prevChromEnd,prevPred,"0",".","0","0",prevColor])+"\n")
                prevChrom = chrom
                prevChromStart = chromStart
                prevChromEnd = chromEnd
                prevPred,prevColor = pred,color
    
    inputFile.close()
    outputFile.close()
    return(bedFileName)
    
def makeBigBedFileAndSymLink(bedFileName,fileName,workDir,assembly,outputColorFile,trackName):
    if trackName == "revel":
        bbSaveFile = "/hive/data/genomes/"+assembly+"/bed/revel/"+fileName+".color.bb"
    elif trackName == "cadd1.7":
        bbSaveFile = "/hive/data/genomes/"+assembly+"/bed/cadd/v1.7/"+fileName+".color.bb"
    elif trackName == "alphaMissense":
        bbSaveFile = "/hive/data/genomes/"+assembly+"/bed/alphaMissense/"+fileName+".color.bb"
    else:
        print("No track name found.")
    bash("bedToBigBed -type=bed9 -tab "+workDir+bedFileName+" /cluster/data/"+assembly+"/chrom.sizes "+bbSaveFile)
    bash("ln -sf "+bbSaveFile+" "+outputColorFile)
    print(outputColorFile)

def main():
    workDir = '/hive/users/lrnassar/temp/colorWig/'
    
    for assembly in ["hg38","hg19"]:
        for fileName in ["a","c","t","g"]:
            for trackName in ["cadd1.7", "revel", "alphaMissense"]:
                fullBigWigFilePath = "/gbdb/"+assembly+"/"+trackName+"/"+fileName+".bw"
                outputColorFile = "/gbdb/"+assembly+"/"+trackName+"/"+fileName+".color.bb"
                convertBigWigToBedGraph(fullBigWigFilePath,workDir,fileName)
                bedFileName = createColorFileFromBigWig(workDir,fileName,trackName)
                makeBigBedFileAndSymLink(bedFileName,fileName,workDir,assembly,outputColorFile,trackName)

main()
