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

rawFilePath = "/hive/data/inside/enigmaTracksData/CSpec_BRCA12ACMG_Rules-Specifications_V1.1_Table-9_2023-11-22-1.txt"
bash("tail -n +4 "+rawFilePath+" > /hive/data/inside/enigmaTracksData/CSpecDataMinusHeader.txt")
rawFileNoHeader = open('/hive/data/inside/enigmaTracksData/CSpecDataMinusHeader.txt','r', encoding='latin-1')
varsToConvertToVcf = open('/hive/data/inside/enigmaTracksData/varsToConvert.txt','w')

def assignRGBcolor(assignedCode):
    if assignedCode == "PS3":
        itemRgb = '0,100,120' #Dark blue
    elif assignedCode == "BS3":
        itemRgb = '0,200,240' #Light blue
    elif assignedCode == "None":
        itemRgb = '0,0,0' #Black
    else:
        print("Error, no code match: "+assignedCode)
    return(itemRgb)

#itemRgb = '0,128,0' #Green

for line in rawFileNoHeader:
    line = line.rstrip().split("\t")
    if line[0].startswith("BRCA1"):
        varsToConvertToVcf.write("NM_007294.4:"+line[1]+"\n")
    elif line[0].startswith("BRCA2"):
        varsToConvertToVcf.write("NM_000059.4:"+line[1]+"\n")
    else:
        print("ERROR! Missing transcript")
varsToConvertToVcf.close()
rawFileNoHeader.close()

bash("hgvsToVcf -noLeftShift hg38 /hive/data/inside/enigmaTracksData/varsToConvert.txt /hive/data/inside/enigmaTracksData/tempVcfFile")

tempVcfFile = open('/hive/data/inside/enigmaTracksData/tempVcfFile','r')

vcfVarCoords = {}
for line in tempVcfFile:
    line = line.rstrip().split("\t")
    if line[0].startswith("#"):
        continue
    else:
        #Have the dic be the hgnsc as key (e.g. NM_007294.3:c.1081T>C) with a list of chr + vcfLocation
        #Also subtracting 1 to make a chr chromStart chromEnd format
        vcfVarCoords[line[2]] = [line[0],str(int(line[1])-1),line[1]]

tempVcfFile.close()

rawFileNoHeader = open('/hive/data/inside/enigmaTracksData/CSpecDataMinusHeader.txt','r', encoding='latin-1')
outputBedFile = open("/hive/data/inside/enigmaTracksData/outputBedFile.bed",'w')
#Reiterate through the file matching coordinates now
for line in rawFileNoHeader:
    line = line.rstrip("\n").split("\t")

    geneSymbol = line[0]
    if line[0].startswith("BRCA1"):
        nmAccession = "NM_007294.4"
    elif line[0].startswith("BRCA2"):
        nmAccession = "NM_000059.4"
    else:
        print("Error: Line didn't start with BRCA*")

    nameToMatch = nmAccession+":"+line[1]
    _mouseOver = geneSymbol+"<br><b>HGVSc:</b> "+nmAccession+\
        ":"+line[1]+"<br><b>HGVSp:</b> "+line[2]+\
        "<br><b>Assigned code:</b> "+line[3]+\
        "<br><b>Code weight:</b> "+line[4]

    itemRGB = assignRGBcolor(line[3])

    if nameToMatch in vcfVarCoords.keys():
        lineToWrite = ("\t".join(vcfVarCoords[nameToMatch])+"\t"+line[1]+\
                            "\t0\t.\t"+"\t".join(vcfVarCoords[nameToMatch][1:])+\
                            "\t"+itemRGB+"\t"+nmAccession+"\t"+line[0]+"\t"+\
                            "\t".join(line[2:])+"\t"+_mouseOver+"\n")

        outputBedFile.write(lineToWrite)
    else:
        print("PROBLEM! A key did not match.")
        print(nameToMatch)

outputBedFile.close()
rawFileNoHeader.close()

bash("bedSort /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/inside/enigmaTracksData/outputBedFile.bed")

startOfAsFile="""table BRCAfunctionalAssays
"BRCA1 and BRCA2 functional assay results reviewed for application of PS3 and BS3 codes"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "HGVS Nucleotide"
   uint score;         "Not used, all 0"
   char[1] strand;     "Not used, all ."
   uint thickStart;    "Same as chromStart"
   uint thickEnd;      "Same as chromEnd"
   uint reserved;       "RGB value (use R,G,B string in input file)"
   string NMaccession;        "RefSeq NM Accession"
   string geneSymbol;        "Gene symbol"
"""

line1 = bash("head -2 "+rawFilePath+" | tail -1")
line2 = bash("head -3 "+rawFilePath+" | tail -1")
line1 = line1.rstrip("\n").split("\t")
line2 = line2.rstrip("\n").split("\t")

name = []
for i in range(14):
    if line2[i] != "":
        line2[i] = line2[i].replace('"','')
        name.append(line1[i]+" - "+line2[i])
    else:
        name.append(line1[i])
n=0
for i in name[2:]:
    n+=1
    asFileAddition = "   lstring extraField"+str(n)+';\t"'+i+'"\n'
    startOfAsFile = startOfAsFile+asFileAddition
startOfAsFile = startOfAsFile+"   string _mouseOver;"+'\t"'+'Field only used as mouseOver'+'"\n'

asFileOutput = open("/hive/data/inside/enigmaTracksData/BRCAfunctionalAssays.as","w")
for line in startOfAsFile.split("\n"):
    if "_mouseOver" in line:
        asFileOutput.write(line+"\n   )")
    else:
        asFileOutput.write(line+"\n")
asFileOutput.close()

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAfunctionalAssays.as -type=bed9+15 -tab \
/hive/data/inside/enigmaTracksData/outputBedFile.bed /cluster/data/hg38/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAfunctionalAssaysHg38.bb")

bash("liftOver -bedPlus=9 -tab /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/genomes/hg38/bed/liftOver/hg38ToHg19.over.chain.gz \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /hive/data/inside/enigmaTracksData/unmapped.bed")

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAfunctionalAssays.as -type=bed9+15 -tab \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /cluster/data/hg19/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAfunctionalAssaysHg19.bb")

bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAfunctionalAssaysHg38.bb /gbdb/hg38/bbi/enigma/BRCAfunctionalAssays.bb")
bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAfunctionalAssaysHg19.bb /gbdb/hg19/bbi/enigma/BRCAfunctionalAssays.bb")
