import re
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

rawFilePath = "/hive/data/inside/enigmaTracksData/Anna_CSpec_BRCA12ACMG-Rules-Specifications_V1.1Table-4_2023-11-22.txt"

def assignRGBcolor(lineToCheck):
    if "DEL" in lineToCheck[4]:
        itemRgb = '180,53,53' #Red
    elif "DUP" in lineToCheck[4]:
        itemRgb = '117,136,214' #Blue
    elif "RNA" in lineToCheck[5]:
        itemRgb = '146,64,190' #Black
    else: 
        itemRgb = '0,0,0'
    return(itemRgb)

rawFile = open(rawFilePath,'r')
outputBedFile = open("/hive/data/inside/enigmaTracksData/outputBedFile.bed",'w', encoding='latin-1')
for line in rawFile:
    line = line.rstrip("\n").split("\t")
    if line[0].startswith("Gene") or line[0]== "":
        continue
    elif line == "":
        continue
    else:
#         print(line)
        itemRgb = assignRGBcolor(line)
        NMacc = line[1]
        
        if line[0] == "BRCA1":
            strand = "-"
            chrom = "chr17"
        else:
            strand = "+"
            chrom = "chr13"
        
        if len(line[3].split("c")) > 2:
            firstPos=line[3].split(".")[1].split("-c")[0]
            secondPos=line[3].split("-c.")[1]
            queryPosition = bash("curl https://hgwdev.gi.ucsc.edu/cgi-bin/hgSearch?search="+NMacc+"%3Ac"+firstPos)
            for resultsLine in queryPosition.split("\n"):
                if resultsLine.startswith("<script"):
                    if strand == "-":
                        chromEnd = str(int(resultsLine.split("position=")[1].split("-")[0].split(":")[1])+5)
                    elif strand == "+":
                        chromStart = str(int(resultsLine.split("position=")[1].split("-")[0].split(":")[1])+4)
            
            queryPosition = bash("curl https://hgwdev.gi.ucsc.edu/cgi-bin/hgSearch?search="+NMacc+"%3Ac"+secondPos)
            for resultsLine in queryPosition.split("\n"):
                if resultsLine.startswith("<script"):
                    if strand == "-":
                        chromStart = str(int(resultsLine.split("position=")[1].split("-")[0].split(":")[1])+4)
                    elif strand == "+":
                        chromEnd = str(int(resultsLine.split("position=")[1].split("-")[0].split(":")[1])+5)

        else:
            p = re.compile('[0-9]')

            if "-" in line[3]: #c.-20-1G>
                if len(line[3].split("-")) > 2:
                    pos = "-"+line[3].split("-")[1]
                    adjustment = p.findall(line[3].split("-")[2])
                    adjustment = "-"+adjustment[0]
                elif line[3].startswith("c.-"): #c.-20+2T>
                    pos = "-"+line[3].split("-")[1].split("+")[0]
                    adjustment = p.findall(line[3].split("+")[1])
                    adjustment = adjustment[0]
                else: #c.20-2T>
                    pos = line[3].split("c.")[1].split("-")[0]
                    adjustment = p.findall(line[3].split("-")[1])
                    adjustment = "-"+adjustment[0]
            
            elif "+" in line[3]: #c.475+1G>
                pos = line[3].split("c.")[1].split("+")[0]
                adjustment = p.findall(line[3].split("+")[1])[0]
            else: #c.1
                pos = line[3].split('c.')[1]
                adjustment = 0
                
            queryPosition = bash("curl https://hgwdev.gi.ucsc.edu/cgi-bin/hgSearch?search="+NMacc+"%3Ac"+pos)
            for resultsLine in queryPosition.split("\n"):
                if resultsLine.startswith("<script"):
                    position = int(resultsLine.split("position=")[1].split("-")[0].split(":")[1])+5
            if strand == "-":
                position = int(position)-int(adjustment)
            elif strand == "+":
                position = int(position)+int(adjustment)
            chromStart = str(position-1)
            chromEnd = str(position)
        
        varType = line[4].replace('"','')
        ACMGcode = line[5].replace('"','')
        observation = line[6].replace("?","delta").replace('"','')
        _mouseOver = "<b>Transcript:</b> "+line[1]+"<br><b>Exon:</b> "+line[2]+\
        "<br><b>Position:</b> "+line[3]+"<br><b>Var Type:</b> "+varType+\
        "<br><b>ACMG Code: </b>"+ACMGcode
        
        outputBedFile.write(chrom+"\t"+chromStart+"\t"+chromEnd+"\t"+line[3]+"\t0\t"+\
                           strand+"\t"+chromStart+"\t"+chromEnd+"\t"+itemRgb+"\t"+\
                           "\t".join(line[:4])+"\t"+varType+"\t"+\
                            ACMGcode+"\t"+observation+\
                            "\t"+line[7]+"\t"+_mouseOver+"\n")
            
rawFile.close()
outputBedFile.close()

bash("bedSort /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/inside/enigmaTracksData/outputBedFile.bed")

startOfAsFile="""table BRCAsplicing
"BRCA1 and BRCA2 variant codes according to PVS1 decision trees (ENIGMA specifications version 1.1.0)"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "Var location"
   uint score;         "Not used, all 0"
   char[1] strand;     "- or +"
   uint thickStart;    "Same as chromStart"
   uint thickEnd;      "Same as chromEnd"
   uint reserved;       "RGB value (use R,G,B string in input file)"
"""

line1 = bash("head -1 "+rawFilePath)
line1 = line1.rstrip("\n").split("\t")

name = []
for i in range(8): 
    asFileAddition = "   lstring extraField"+str(i)+';\t"'+line1[i]+'"\n'
    startOfAsFile = startOfAsFile+asFileAddition
startOfAsFile = startOfAsFile+"   string _mouseOver;"+'\t"'+'Field only used as mouseOver'+'"\n'
    
asFileOutput = open("/hive/data/inside/enigmaTracksData/BRCAsplicing.as","w")
for line in startOfAsFile.split("\n"):
    if "_mouseOver" in line:
        asFileOutput.write(line+"\n   )")
    else:
        asFileOutput.write(line+"\n")
asFileOutput.close()

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAsplicing.as -type=bed9+9 -tab \
/hive/data/inside/enigmaTracksData/outputBedFile.bed /cluster/data/hg38/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAsplicingHg38.bb")

bash("liftOver -bedPlus=9 -tab /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/genomes/hg38/bed/liftOver/hg38ToHg19.over.chain.gz \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /hive/data/inside/enigmaTracksData/unmapped.bed")

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAsplicing.as -type=bed9+9 -tab \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /cluster/data/hg19/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAsplicingHg19.bb")

bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAsplicingHg38.bb /gbdb/hg38/bbi/enigma/BRCAsplicing.bb")
bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAsplicingHg19.bb /gbdb/hg19/bbi/enigma/BRCAsplicing.bb")
