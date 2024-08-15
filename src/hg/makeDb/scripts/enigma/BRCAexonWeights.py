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

rawFilePath = "/hive/data/inside/enigmaTracksData/CSpec_BRCA12ACMG_Rules-SupplementaryTables_V1.1_2023-11-22-1.txt"
bash("tail -n +4 "+rawFilePath+" > /hive/data/inside/enigmaTracksData/CSpecRulesDataMinusHeader.txt")
rawFileNoHeader = open('/hive/data/inside/enigmaTracksData/CSpecRulesDataMinusHeader.txt','r', encoding='latin-1')
outputBedFile = open("/hive/data/inside/enigmaTracksData/outputBedFile.bed",'w')

#Reiterate through the file matching coordinates now
for line in rawFileNoHeader:
    line = line.rstrip("\n").split("\t")
    if line[0] == "BRCA1":
        strand = "-"
        chrom = "chr17"
        NMacc = "NM_007294.4"
    else:
        strand = "+"
        chrom = "chr13"
        NMacc = "NM_000059.4"
    
    geneSymbol = line[0]
    _mouseOver = "<b>Transcript: </b>"+NMacc+\
        "<br><b>Exon number:</b> "+line[1]+\
        "<br><b>PM5_PTC code strength:</b> "+line[11]
    
    
    lineToWrite = (chrom+"\t"+str(int(line[8])-1)+"\t"+line[9]+"\t"+line[0]+" - "+line[1]+\
                   "\t0\t"+strand+"\t"+str(int(line[8])-1)+"\t"+line[9]+"\t"+NMacc+\
                   "\t"+"\t".join(line[1:6])+\
                   "\t".join(line[9:])+\
                   "\t"+_mouseOver+"\n")
        
    outputBedFile.write(lineToWrite)

            
outputBedFile.close()
rawFileNoHeader.close()

bash("bedSort /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/inside/enigmaTracksData/outputBedFile.bed")

startOfAsFile="""table BRCAexonWeights
"BRCA1 and BRCA2 exon-specific weights"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "Gene symbol - exon # (total exon count)"
   uint score;         "Not used, all 0"
   char[1] strand;     "- or +"
   uint thickStart;    "Same as chromStart"
   uint thickEnd;      "Same as chromEnd"
   string NMaccession; "NCBI NM isoform accession"
"""
n=0
file = open(rawFilePath,'r', encoding='latin-1')
for line in file:
    n+=1
    if n==2:
        line1 = line
    elif n==3:
        line2 = line
file.close()

line1 = line1.rstrip("\n").split("\t")
line2 = line2.rstrip("\n").split("\t")

fieldsOfInterest = [1,2,3,4,5,10,11,12,13,14,15,16,17,18]
name = []
for i in fieldsOfInterest:
    if line2[i] != "":
        line2[i] = line2[i].replace('"','')
        name.append(line1[i]+" - "+line2[i])
    else:
        name.append(line1[i])
n=0
for i in name:
    n+=1 
    asFileAddition = "   lstring extraField"+str(n)+';\t"'+i+'"\n'
    startOfAsFile = startOfAsFile+asFileAddition
startOfAsFile = startOfAsFile+"   string _mouseOver;"+'\t"'+'Field only used as mouseOver'+'"\n'
    
asFileOutput = open("/hive/data/inside/enigmaTracksData/BRCAexonWeights.as","w")
for line in startOfAsFile.split("\n"):
    if "_mouseOver" in line:
        asFileOutput.write(line+"\n   )")
    else:
        asFileOutput.write(line+"\n")
asFileOutput.close()

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAexonWeights.as -type=bed8+15 -tab \
/hive/data/inside/enigmaTracksData/outputBedFile.bed /cluster/data/hg38/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAexonWeightsHg38.bb")

bash("liftOver -bedPlus=8 -tab /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/genomes/hg38/bed/liftOver/hg38ToHg19.over.chain.gz \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /hive/data/inside/enigmaTracksData/unmapped.bed")

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAexonWeights.as -type=bed8+15 -tab \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /cluster/data/hg19/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAexonWeightsHg19.bb")

bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAexonWeightsHg38.bb /gbdb/hg38/bbi/enigma/BRCAexonWeights.bb")
bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAexonWeightsHg19.bb /gbdb/hg19/bbi/enigma/BRCAexonWeights.bb")
