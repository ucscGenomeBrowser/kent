from collections import OrderedDict

#chr20   68354   A       C       ENST00000382410 2       missense_variant        16      0.75    0.725   1.034   1       70.701
#Chromosome_name-Genomic_position-ref-alt-Feature-Protein_position-Consequence-missyn_tally-mtr_obs-mtr_exp-MTR-FDR-MTR_centile
mtrFile = open("/hive/data/outside/mtr/mtrUcsc.txt",'r')
outFile = open("/hive/data/outside/mtr/mtr2.bed",'w')

#Items in the bottom 25centile = red, middle 50 = black, top 25 = green, and no score = blue
def assignRGBcolorByMTRcentile(mtrCentile):
    """Assign color based on mtr percentile"""
    
    if float(mtrCentile) < 25:
        itemRgb = '255,0,0'
    elif float(mtrCentile) > 75:
        itemRgb = '0,128,0'
    else:
        itemRgb = '0,0,0'
    return(itemRgb)

mtrScoreDic = OrderedDict()

prevChrom = ""
for line in mtrFile:
    line = line.rstrip().split("\t")
    chrom = line[0]
    if prevChrom != chrom:
        print("On new chrom: "+chrom)
    chromStart = int(line[1])-1
    chromEnd = int(line[1])
    ref = line[2]
    alt = line[3]
    name = line[4]
    proteinPos = line[5]
    consequence = line[6]
    itemRgb = '0,0,255'
    missynTally = ""
    obsRatio = ""
    expRatio = ""
    MTR = ""
    FDR = ""
    mtrCentile = ""
    if len(line) == 8:
        missynTally = line[7]
    elif len(line) == 10:
        missynTally = line[7]
        obsRatio = line[8]
        expRatio = line[9]  
    elif len(line) == 13:
        missynTally = line[7]
        obsRatio = line[8]
        expRatio = line[9]
        MTR = line[10]
        FDR = line[11]
        mtrCentile = line[12]
        itemRgb = assignRGBcolorByMTRcentile(mtrCentile)
        
    outFile.write(str(chrom)+"\t"+str(chromStart)+"\t"+str(chromEnd)+"\t"+name+"\t0\t.\t"+str(chromStart)+"\t"\
                      +str(chromEnd)+"\t"+itemRgb+"\t"+ref+"\t"+alt+"\t"+str(proteinPos)+"\t"+consequence+"\t"\
                      +str(missynTally)+"\t"+str(obsRatio)+"\t"+str(expRatio)+"\t"+str(MTR)+"\t"+str(FDR)+"\t"+\
                      str(mtrCentile)+"\n")
    prevChrom = chrom
        
outFile.close()
mtrFile.close()
