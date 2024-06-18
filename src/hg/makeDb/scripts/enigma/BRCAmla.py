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

def assignRGBcolorByLR(LR):
    LR = float(LR)
    if LR > 2.08:
        itemRgb = '128,64,13' #brown
    elif LR <=0.48:
        itemRgb = '252,157,3' #orange
    else:
        itemRgb = '91,91,91' #grey
    return(itemRgb)

def assignACMGcode(LR):
    LR = float(LR)
    if LR > 2.08 and LR <= 4.3:
        ACMGcode = 'PP4 - Pathogenic - Supporting'
    elif LR > 4.3 and LR <= 18.7:
        ACMGcode = 'PP4 - Pathogenic - Moderate'
    elif LR > 18.7 and LR <= 350:
        ACMGcode = 'PP4 - Pathogenic - Strong'
    elif LR > 350:
        ACMGcode = 'PP4 - Pathogenic - Very strong'
    elif LR <= .48 and LR > .23:
        ACMGcode = 'BP5 - Benign - Supporting'
    elif LR <= .22 and LR > .05:
        ACMGcode = 'BP5 - Benign - Moderate'
    elif LR <= .04 and LR > .00285:
        ACMGcode = 'BP5 - Benign - Strong'
    elif LR <= 0.00285:
        ACMGcode = 'BP5 - Benign - Very strong'
    else:
        print("This code did not match: "+str(LR))
        ACMGcode = "Not informative"
    return(ACMGcode)


bash("tail -n +3 /hive/data/inside/enigmaTracksData/HUMU-40-1557-s001-1.txt > /hive/data/inside/enigmaTracksData/HUMUdataMinusHeader.txt")
rawFileNoHeader = open('/hive/data/inside/enigmaTracksData/HUMUdataMinusHeader.txt','r', encoding='latin-1')
varsToConvertToVcf = open('/hive/data/inside/enigmaTracksData/varsToConvert.txt','w')

for line in rawFileNoHeader:
    line = line.rstrip().split("\t")
    if line[0].startswith("BRCA1"):
        varsToConvertToVcf.write("NM_007294.3:"+line[1]+"\n")
    elif line[0].startswith("BRCA2"):
        varsToConvertToVcf.write("NM_000059.3:"+line[1]+"\n")
    else:
        print("ERROR! Missing transcript")

rawFileNoHeader.close()
varsToConvertToVcf.close()

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

rawFileNoHeader = open('/hive/data/inside/enigmaTracksData/HUMUdataMinusHeader.txt','r', encoding='latin-1')
outputBedFile = open("/hive/data/inside/enigmaTracksData/outputBedFile.bed",'w')
#Reiterate through the file matching coordinates now
for line in rawFileNoHeader:
    line = line.rstrip("\n").split("\t")

    geneSymbol = line[0]
    if line[0].startswith("BRCA1"):
        nmAccession = "NM_007294.3"
    elif line[0].startswith("BRCA2"):
        nmAccession = "NM_000059.3"
    else:
        print("Error: Line didn't start with BRCA*")

    ACMGcode = assignACMGcode(line[10])
    nameToMatch = nmAccession+":"+line[1]
    _mouseOver = "<b>HGVSc:</b> "+nmAccession+":"+line[1]+"<br><b>HGVSp:</b> "+\
        line[2]+"<br><b>Combined LR:</b> "+line[10]+\
        "<br><b>ACMG Code:</b> "+ACMGcode

    itemRGB = assignRGBcolorByLR(line[10])

    if nameToMatch in vcfVarCoords.keys():
        lineToWrite = ("\t".join(vcfVarCoords[nameToMatch])+"\t"+line[1]+\
                            "\t0\t.\t"+"\t".join(vcfVarCoords[nameToMatch][1:])+\
                            "\t"+itemRGB+"\t"+nmAccession+"\t"+line[0]+"\t"+\
                            ACMGcode+"\t"+"\t".join(line[2:])+"\t"+_mouseOver+"\n")

        outputBedFile.write(lineToWrite)
    else:
        print("PROBLEM! A key did not match.")
        print(nameToMatch)

outputBedFile.close()
rawFileNoHeader.close()

bash("bedSort /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/inside/enigmaTracksData/outputBedFile.bed")

startOfAsFile="""table BRCAmla
"BRCA1/BRCA2 multifactorial likelihood analysis"
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
   string ACMGcode;        "Translating the LR into the ACMG codes PP4 and BP5 alongside supporting strength."
"""

line1 = bash("head -1 /hive/data/inside/enigmaTracksData/HUMU-40-1557-s001-1.txt")
line2 = bash("head -2 /hive/data/inside/enigmaTracksData/HUMU-40-1557-s001-1.txt | tail -1")
line1 = line1.rstrip("\n").split("\t")
line2 = line2.rstrip("\n").split("\t")

name = []
for i in range(65):
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

asFileOutput = open("/hive/data/inside/enigmaTracksData/BRCAmla.as","w")
for line in startOfAsFile.split("\n"):
    if "_mouseOver" in line:
        asFileOutput.write(line+"\n   )")
    else:
        asFileOutput.write(line+"\n")
asFileOutput.close()

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAmla.as -type=bed9+67 -tab \
/hive/data/inside/enigmaTracksData/outputBedFile.bed /cluster/data/hg38/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAmfaHg38.bb")

bash("liftOver -bedPlus=9 -tab /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/genomes/hg38/bed/liftOver/hg38ToHg19.over.chain.gz \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /hive/data/inside/enigmaTracksData/unmapped.bed")

bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAmla.as -type=bed9+67 -tab \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /cluster/data/hg19/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAmfaHg19.bb")

bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAmfaHg38.bb /gbdb/hg38/bbi/enigma/BRCAmfa.bb")
bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAmfaHg19.bb /gbdb/hg19/bbi/enigma/BRCAmfa.bb")
