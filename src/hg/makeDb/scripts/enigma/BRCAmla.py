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

def parseAliasMappingFile(fileUrl):
    aliasMappingDic = {}
    aliasMappingFile = open(fileUrl,'r', encoding='latin-1')
    for line in aliasMappingFile:
        line = line.rstrip()
        if line.startswith("Source"):
            continue
        else:
            line = line.split("\t")
            HGVSname = line[356].replace("NM_007294.3", "NM_007294.4").replace("NM_000059.3", "NM_000059.4")
            bicNom = line[340].split(",")[0]
            protChange = line[337]
            synonyms = line[351].split(",")

            if HGVSname not in aliasMappingDic.keys():
                aliasMappingDic[HGVSname]={}
            aliasMappingDic[HGVSname]["bicNom"]= bicNom
            aliasMappingDic[HGVSname]["protChange"]= protChange
            aliasMappingDic[HGVSname]["synonyms"]= synonyms
    aliasMappingFile.close()
    return(aliasMappingDic)

def processEastonFile(fileUrl,eastonVarsToMapDic):
    eastonFile = open(fileUrl,'r', encoding='latin-1')
    for line in eastonFile:
        line = line.rstrip()
        if line.startswith("BRCA Sequence") or line.startswith("\t") or line.startswith("Gene and Variant") or line.startswith("BRCA1:") or line.startswith("BRCA2:") or line.startswith("Variants with"):
            continue
        else:
            line = line.split("\t")
            varName = line[0].replace("?", ">").split(" ")[0]
            familyLR = 10**(float(line[1].replace("?", "-")))
            coocurrenceLR = 10**(float(line[2].replace("?", "-")))
            segregationLR = 10**(float(line[3].replace("?", "-")))

            if varName not in eastonVarsToMapDic.keys():
                eastonVarsToMapDic[varName] = {}
                eastonVarsToMapDic[varName]["familyLR"] = familyLR
                eastonVarsToMapDic[varName]["coocurrenceLR"] = coocurrenceLR
                eastonVarsToMapDic[varName]["segregationLR"] = segregationLR
            else:
                print("This should not happen. Bad var: "+varName)
    eastonFile.close()
    return(eastonVarsToMapDic)

def parseEastonData(fileUrl1,fileUrl2):
    eastonVarsToMapDic = {}
    eastonVarsToMapDic = processEastonFile(fileUrl1,eastonVarsToMapDic)
    eastonVarsToMapDic = processEastonFile(fileUrl2,eastonVarsToMapDic)
    return(eastonVarsToMapDic)

def processVar(foundVar,n,eastonVarsDic,eastonVarsToMapDic,var,key):
    foundVar = True
    eastonVarsDic[key] = {}
    for LRs in eastonVarsToMapDic[var].keys():
        eastonVarsDic[key][LRs] = eastonVarsToMapDic[var][LRs]
    n+=1
    return(foundVar,n,eastonVarsDic)

def associateEastonWithAlias(eastonVarsToMapDic,aliasMappingDic):
    eastonVarsDic = {}
    n=0
    for var in eastonVarsToMapDic.keys():
        foundVar = False
        for key in aliasMappingDic.keys():
            if foundVar == False:
                if var in aliasMappingDic[key]["bicNom"]:
                    foundVar,n,eastonVarsDic = processVar(foundVar,n,eastonVarsDic,eastonVarsToMapDic,var,key)
                elif var in aliasMappingDic[key]["protChange"]:
                    foundVar,n,eastonVarsDic = processVar(foundVar,n,eastonVarsDic,eastonVarsToMapDic,var,key)
                elif var in aliasMappingDic[key]["synonyms"]:
                    foundVar,n,eastonVarsDic = processVar(foundVar,n,eastonVarsDic,eastonVarsToMapDic,var,key)
    if len(eastonVarsToMapDic.keys()) != n:
        print("Error: Some Easton variants did not map. Vars mapped: "+str(n)+ " Total vars: "+str(len(eastonVarsToMapDic.keys())))
    return(eastonVarsDic)

def createVar(varsDic,line,geneSymbol,dicName):
    if dicName == "parsonsVarsDic":
        varName = geneSymbol+line[1]
        varsDic[varName] = {}
        if line[5] != '':
            varsDic[varName]["segregationLR"] = line[5]
        if line[6] != '':
            varsDic[varName]["pathologyLR"] = line[6]
        if line[7] != '':
            varsDic[varName]["coocurrenceLR"] = line[7]
        if line[8] != '':
            varsDic[varName]["familyLR"] = line[8]
        if line[9] != '':
            varsDic[varName]["caseControlLR"] = line[9]
    elif dicName == "caputoVarsDic":
        varName = geneSymbol+line[2]
        varsDic[varName] = {}
        if line[13] != '' and line[13] != '–':
            varsDic[varName]["segregationLR"] = line[13].replace('"',"").replace(",","")
        if line[8] != '' and line[8] != '–':
            varsDic[varName]["pathologyLR"] = line[8].replace('"',"").replace(",","")
        if line[7] != '' and line[7] != '–':
            varsDic[varName]["coocurrenceLR"] = line[7].replace('"',"").replace(",","")
        if line[6] != '' and line[6] != '–':
            varsDic[varName]["familyLR"] = line[6].replace('"',"").replace(",","")
    elif dicName == "liVarsDic":
        varName = geneSymbol+line[2]
        varsDic[varName] = {}
        if line[5] != '' and line[5] != '-':
            varsDic[varName]["familyLR"] = line[5]
    return(varsDic)

def parseParsonsOrCaputoOrLiData(fileUrl,dicName):
    varsDic = {}
    if dicName == "caputoVarsDic":
        openFile = open(fileUrl,'r', encoding='utf-16')
    else:
        openFile = open(fileUrl,'r', encoding='latin-1')
    for line in openFile:
        line = line.rstrip()
        if line.startswith("\"Refere") or line.startswith("\t") or line.startswith("Gene") or line.startswith("GENE"):
            continue
        else:
            line = line.split("\t")
            if line[0].startswith("BRCA1"):
                varsDic = createVar(varsDic,line,"NM_007294.4:",dicName)
            elif line[0].startswith("BRCA2"):
                varsDic = createVar(varsDic,line,"NM_000059.4:",dicName)
            else:
                print("ERROR! Missing transcript: ")
                print(line)
    openFile.close()
    keysToDelete = []
    for key in varsDic.keys():
        if varsDic[key] == {}:
            print("Empty var, deleting: "+dicName+" - "+key)
            keysToDelete.append(key)
    for key in keysToDelete:
        del varsDic[key]
    return(varsDic)

def sanitizeEastonVarsDic(eastonVarsDic,parsonsVarsDic):
    repeatedVars = []
    for key in eastonVarsDic.keys():
        if key in parsonsVarsDic.keys():
            repeatedVars.append(key)
    for variant in repeatedVars:
        del eastonVarsDic[variant]
    print(str(len(repeatedVars))+" variants removed from Easton data because they were also found in Parsons.")
    return(eastonVarsDic)

def parseDicsAndCreateFinalLLRdic(caputoVarsDic,parsonsVarsDic,liVarsDic,eastonVarsDic):
    combinedDic = {'caputoVarsDic' : caputoVarsDic, 'parsonsVarsDic' : parsonsVarsDic, 'liVarsDic' : liVarsDic, 'eastonVarsDic' : eastonVarsDic}
    finalVarsList = set()
    allLRsPossible = ["segregationLR","pathologyLR","coocurrenceLR","familyLR","caseControlLR"]
    finalCombinedLRdic = {}
    for dic in combinedDic.keys():
        for key in combinedDic[dic].keys():
            finalVarsList.add(key)
    for variant in finalVarsList:
        finalCombinedLRdic[variant] = {}
        for dic in combinedDic.keys():
            if variant in combinedDic[dic].keys():
                finalCombinedLRdic[variant][dic] = {}
                for LR in allLRsPossible:
                    if LR in combinedDic[dic][variant]:
                        if LR+"combined" in finalCombinedLRdic[variant].keys(): #Look for familyLR in parsonsXXX
                            #Assign the combined value
                            finalCombinedLRdic[variant][LR+"combined"] = finalCombinedLRdic[variant][LR+"combined"] * float(combinedDic[dic][variant][LR])
                            #Assign individual value
                            finalCombinedLRdic[variant][dic][LR] = float(combinedDic[dic][variant][LR])
                        else:
                            finalCombinedLRdic[variant][dic][LR] = float(combinedDic[dic][variant][LR])
                            #First value for the combined
                            finalCombinedLRdic[variant][LR+"combined"] = float(combinedDic[dic][variant][LR])
                            
    for var in finalCombinedLRdic.keys():
        finalCombinedLRdic[var]["combinedLRscore"] = 1
        for combinedLR in finalCombinedLRdic[var].keys():
            if combinedLR.endswith("combined"):
                finalCombinedLRdic[var]["combinedLRscore"] = finalCombinedLRdic[var]["combinedLRscore"] * finalCombinedLRdic[var][combinedLR]

    print("Total number of final variables in combined dataset: "+str(len(finalVarsList)))
    #The result is a dictionary as such:
    #Level 1: Variants, e.g. NM_000059.4:c.3509C>T
    #Level 2: A dictionary for each dataset, a combined score for each LR (5 total), and a final combinedLR from multiplying all combined individual scores
    #Level 3: The only level 3 dic is each individual dataset have their value recorded for each LR, e.g. 'parsonsVarsDic': {'coocurrenceLR': 1.1570849263,'familyLR': 1.6809221375,}
    return(finalCombinedLRdic)

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
#         print("This code did not match: "+str(LR))
        ACMGcode = "Not informative"
    return(ACMGcode)

def checkVarAndWriteToLine(potentialDataset,combinedLRdic,variant,dataset,listOfValues,lineToWrite):
    if dataset in combinedLRdic[variant].keys():
        if potentialDataset in combinedLRdic[variant][dataset].keys():
            listOfValues.append(str(round(combinedLRdic[variant][dataset][potentialDataset],5)))
        else:
            listOfValues.append("NULL")
    return(lineToWrite)

def createOutputBedLine(combinedLRdic,vcfVarCoords):
    outputBedFile = open("/hive/data/inside/enigmaTracksData/outputBedFile.bed",'w')
    n=0
    for variant in combinedLRdic.keys():
        ACMGcode = assignACMGcode(combinedLRdic[variant]["combinedLRscore"])
        _mouseOver = "<b>HGVSc:</b> "+variant+"<br>"+\
            "<b>Combined LR:</b> "+str(round(combinedLRdic[variant]["combinedLRscore"],5))+\
            "<br><b>ACMG Code:</b> "+ACMGcode
        itemRGB = assignRGBcolorByLR(combinedLRdic[variant]["combinedLRscore"])

        if variant in vcfVarCoords.keys():
            #first three fields chrom, chromStart, chromEnd- then next 5 standard bed fields
            lineToWrite = ("\t".join(vcfVarCoords[variant])+\
            "\t"+variant+"\t0\t.\t"+"\t".join(vcfVarCoords[variant][1:])+\
            "\t"+itemRGB+"\t"+str(round(combinedLRdic[variant]["combinedLRscore"],5))+"\t"+\
            ACMGcode+"\t")

            #Add the combined LRs
            for LRcombined in ["familyLRcombined","coocurrenceLRcombined","segregationLRcombined","pathologyLRcombined","caseControlLRcombined"]:
                if LRcombined in combinedLRdic[variant].keys():
                    lineToWrite = lineToWrite+str(round(combinedLRdic[variant][LRcombined],5))+"\t"
                else:
                    lineToWrite = lineToWrite+"\t"

            for dataset in ['caputoVarsDic','parsonsVarsDic','liVarsDic','eastonVarsDic']:
                if dataset == 'caputoVarsDic':
                    listOfValues = []
                    for potentialDataset in ["familyLR","coocurrenceLR","segregationLR","pathologyLR"]:
                        lineToWrite = checkVarAndWriteToLine(potentialDataset,combinedLRdic,variant,dataset,listOfValues,lineToWrite)
                    lineToWrite = lineToWrite+",".join(listOfValues)+'\t'
                elif dataset == 'parsonsVarsDic':
                    listOfValues = []
                    for potentialDataset in ["familyLR","coocurrenceLR","segregationLR","pathologyLR","caseControlLR"]:
                        lineToWrite = checkVarAndWriteToLine(potentialDataset,combinedLRdic,variant,dataset,listOfValues,lineToWrite)
                    lineToWrite = lineToWrite+",".join(listOfValues)+'\t'
                elif dataset == 'liVarsDic':
                    listOfValues = []
                    for potentialDataset in ["familyLR"]:
                        lineToWrite = checkVarAndWriteToLine(potentialDataset,combinedLRdic,variant,dataset,listOfValues,lineToWrite)
                    lineToWrite = lineToWrite+",".join(listOfValues)+'\t'
                elif dataset == 'eastonVarsDic':
                    listOfValues = []
                    for potentialDataset in ["familyLR","coocurrenceLR","segregationLR"]:
                        lineToWrite = checkVarAndWriteToLine(potentialDataset,combinedLRdic,variant,dataset,listOfValues,lineToWrite)
                    lineToWrite = lineToWrite+",".join(listOfValues)+'\t'

            lineToWrite = lineToWrite+_mouseOver+"\n"
            outputBedFile.write(lineToWrite)

        else:
            n+=1
            print(variant)
    print("A total of "+str(n)+" variants did not match.")
    outputBedFile.close()
    
def mapVariantsHGVSandMakeBedFile(combinedLRdic):
    varsToConvertToVcf = open('/hive/data/inside/enigmaTracksData/varsToConvert.txt','w')
    for variant in combinedLRdic.keys():
        varsToConvertToVcf.write(variant+"\n")
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
    createOutputBedLine(combinedLRdic,vcfVarCoords)

def createFinalBigBedFile():
    bash("bedSort /hive/data/inside/enigmaTracksData/outputBedFile.bed \
    /hive/data/inside/enigmaTracksData/outputBedFile.bed")

    asFile="""table BRCAmla
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
   uint reserved;      "RGB value (use R,G,B string in input file)"
   string LLR;         "Combined LR Score"
   string ACMGcode;    "Translating the LR into the ACMG codes PP4 and BP5 alongside supporting strength."
   string familyHistoryCombinedLR;    "Combined LR of all family history scores. Blank if none available."
   string cooccurrenceCombinedLR;    "Combined LR of all co-occurrence scores. Blank if none available."
   string segregationCombinedLR;    "Combined LR of all segregation scores scores. Blank if none available."
   string pathologyCombinedLR;    "Combined LR of all pathology scores scores. Blank if none available."
   string caseControlCombinedLR;    "Combined LR of all case control scores scores. Blank if none available."
   string caputoLRs;    "Comma-separated list of Caputo et al scores, NULL when no score is available. Order is family LR, co-occurrence LR, segregation LR, pathology LR."
   string parsonsLRs;    "Comma-separated list of Parson et al scores, NULL when no score is available. Order is family LR, co-occurrence LR, segregation LR, pathology LR, case control LR."
   string liLRs;    "Comma-separated list of Li et al scores, NULL when no score is available. The only score is family LR."
   string eastonLRs;    "Comma-separated list of Easton et al scores, NULL when no score is available. Order is family LR, co-occurrence LR, segregation LR."
   string _mouseOver;  "Field only used as mouseOver"
"""

    asFileOutput = open("/hive/data/inside/enigmaTracksData/BRCAmla.as","w")
    for line in asFile.split("\n"):
        if "_mouseOver" in line:
            asFileOutput.write(line+"\n   )")
        else:
            asFileOutput.write(line+"\n")
    asFileOutput.close()

    bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAmla.as -type=bed9+12 -tab \
/hive/data/inside/enigmaTracksData/outputBedFile.bed /cluster/data/hg38/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAmfaHg38.bb")

    bash("liftOver -bedPlus=9 -tab /hive/data/inside/enigmaTracksData/outputBedFile.bed \
/hive/data/genomes/hg38/bed/liftOver/hg38ToHg19.over.chain.gz \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /hive/data/inside/enigmaTracksData/unmapped.bed")

    bash("bedToBigBed -as=/hive/data/inside/enigmaTracksData/BRCAmla.as -type=bed9+12 -tab \
/hive/data/inside/enigmaTracksData/outputBedFileHg19.bed /cluster/data/hg19/chrom.sizes \
/hive/data/inside/enigmaTracksData/BRCAmfaHg19.bb")

    bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAmfaHg38.bb /gbdb/hg38/bbi/enigma/BRCAmfa.bb")
    bash("ln -sf /hive/data/inside/enigmaTracksData/BRCAmfaHg19.bb /gbdb/hg19/bbi/enigma/BRCAmfa.bb")

#Build 3 basic dictionaries, these are all simiar
caputoVarsDic = parseParsonsOrCaputoOrLiData("/hive/data/inside/enigmaTracksData/LRtrack/DataFromCaputo2021,34597585.txt","caputoVarsDic")
parsonsVarsDic = parseParsonsOrCaputoOrLiData("/hive/data/inside/enigmaTracksData/HUMU-40-1557-s001-1.txt","parsonsVarsDic")
liVarsDic = parseParsonsOrCaputoOrLiData("/hive/data/inside/enigmaTracksData/LRtrack/DataFromLi2020,31853058.txt","liVarsDic")

#Process the Easton et al data - this is more involved due to data being natural log base 10 and having older HGVS nomenclature
eastonVarsToMapDic = parseEastonData("/hive/data/inside/enigmaTracksData/LRtrack/DataFromEaston2007,17924331_sheet1.txt","/hive/data/inside/enigmaTracksData/LRtrack/DataFromEaston2007,17924331_sheet2.txt")
aliasMappingDic = parseAliasMappingFile("/hive/data/inside/enigmaTracksData/LRtrack/built_with_change_types.tsv")
eastonVarsDic = associateEastonWithAlias(eastonVarsToMapDic,aliasMappingDic)
eastonVarsDic = sanitizeEastonVarsDic(eastonVarsDic,parsonsVarsDic)

#Final dictionary with the combined LLRs for all variants
combinedLRdic = parseDicsAndCreateFinalLLRdic(caputoVarsDic,parsonsVarsDic,liVarsDic,eastonVarsDic)

#Map the variants and create the bed file
mapVariantsHGVSandMakeBedFile(combinedLRdic)

#Make the final bigBed file and symlinks
createFinalBigBedFile()
