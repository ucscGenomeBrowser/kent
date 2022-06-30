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

#Iterate through file and create dictionary combination of chrom+chromEnd with a list of data values
#Reason for this is that a few (~.42%) of items have overlap. We want to take the smallest possible score
#So as to have the highest sensitivity
def parseFileBuildDic(metaDomeFile):
    n=0
    chrom=""
    metaDome = {}
    for line in metaDomeFile:
    #chr1    69091   69093   +       sw_dn_ds:0.16666666666666669,sw_coverage:0.5238095238095238
        if line.startswith("chrom"):
            continue
        else:
            line = line.rstrip()
            line = line.split("\t")
        
            if line[0] != chrom:
                chrom = line[0]
                chromStart = str(int(line[1])-1)
                chromEnd = line[2]
                dataValue = round(float(line[4].split(':')[1].split(",")[0]),2)
                name = chrom+"\t"+chromEnd
                if name not in metaDome.keys():
                    metaDome[name]={}
                    metaDome[name]['name']=chrom+"\t"+chromStart+"\t"+chromEnd
                    metaDome[name]['dataValue']=[]
                metaDome[name]['dataValue'].append(dataValue)
            elif int(line[1]) <= int(chromEnd):
                metaDome[name]['dataValue'].append(dataValue)
                n+=1
            else:
                chrom = line[0]
                chromStart = str(int(line[1])-1)
                chromEnd = line[2]
                dataValue = round(float(line[4].split(':')[1].split(",")[0]),2)
                name = chrom+"\t"+chromEnd
                if name not in metaDome.keys():
                    metaDome[name]={}
                    metaDome[name]['name']=chrom+"\t"+chromStart+"\t"+chromEnd
                    metaDome[name]['dataValue']=[]
                metaDome[name]['dataValue'].append(dataValue)
    print("Total number of items in track: "+str(n))
    return(metaDome)
        

# Iterate dictionary and write out results file
def writeOutFile(metaDome,resultsFile):
    for key, value in metaDome.items():
        resultsFile.write(metaDome[key]['name']+"\t"+str(min(metaDome[key]['dataValue']))+"\n")

#Create files
def createBigWig(outputFile,bigWigFile):
    bash("sort -k1,1 -k2,2n "+outputFile+" > "+outputFile+".sorted")
    bash("bedGraphToBigWig "+outputFile+".sorted /hive/data/genomes/hg19/bed/metaDome/hg19.chrom.sizes "+bigWigFile)
    
def makeMetaDomeBigBed(metaDomeFile,bigBedOutputFile):
    for line in metaDomeFile:
    #chr1    69091   69093   +       sw_dn_ds:0.16666666666666669,sw_coverage:0.5238095238095238
        if line.startswith("chrom"):
            continue
        else:
            line = line.rstrip()
            line = line.split("\t")
            chrom = line[0]
            chromStart = line[1]
            chromEnd = line[2]
            name = chrom+"-"+chromStart
            strand = line[3]
            toleranceScore = str(round(float(line[4].split(",")[0].split(":")[1]),3))
            coverage = str(round(float(line[4].split(",")[1].split(",")[0].split(":")[1]),3))
            #bed 4+3
            bigBedOutputFile.write(chrom+"\t"+chromStart+"\t"+chromEnd+"\t"+name+"\t"+toleranceScore+"\t"+strand+"\t"+coverage+"\n")

    bash("sort -k1,1 -k2,2n /hive/data/genomes/hg19/bed/metaDome/metaDome > /hive/data/genomes/hg19/bed/metaDome/metaDome.sorted.bed")
    bash("bedToBigBed -type=bed4+3 -as=/hive/data/genomes/hg19/bed/metaDome/metaDome.as /hive/data/genomes/hg19/bed/metaDome/metaDome.sorted.bed /hive/data/genomes/hg19/bed/metaDome/hg19.chrom.sizes /hive/data/genomes/hg19/bed/metaDome/metaDome.bb")
      

inputFile = "/hive/data/genomes/hg19/bed/metaDome/MetaDome_v1.0.1_gnomAD_r2.0.2_dnds_sw_size_10_052022.sorted.bed"
outputFile = "/hive/data/genomes/hg19/bed/metaDome/metaDome.bedGraph"
bigWigFile = "/hive/data/genomes/hg19/bed/metaDome/metaDome.bw"
bigBedOutputFile = open("/hive/data/genomes/hg19/bed/metaDome/metaDome","w")


metaDomeFile = open(inputFile,'r')
resultsFile = open(outputFile,'w')

metaDome = parseFileBuildDic(metaDomeFile)
writeOutFile(metaDome,resultsFile)
makeMetaDomeBigBed(metaDomeFile,bigBedOutputFile)
metaDomeFile.close()
resultsFile.close()
bigBedOutputFile.close()
createBigWig(outputFile,bigWigFile)

bash("ln -s /hive/data/genomes/hg19/bed/metaDome/metaDome.bw /gbdb/hg19/metaDome/")
bash("ln -s /hive/data/genomes/hg19/bed/metaDome/metaDome.bb /gbdb/hg19/metaDome/")
