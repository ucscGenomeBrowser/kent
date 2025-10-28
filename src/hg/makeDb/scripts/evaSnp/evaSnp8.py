import re, os, subprocess, sys

def findAllAccFromDescPages():
    """Search all gbdb description pages and pull out GCA/GCF accessions"""
    listOfHits = []
    assemblyAlias = {}
    allDescPages = bash("ls /gbdb/*/html/description.html").rstrip().split("\n")
    regex = re.compile(r'GC[AF]_[0-9]*\.[0-9]*')
    for page in allDescPages:
        try:
            hits = bash("cat "+page+" | grep -i 'GCA\\|GCF'").rstrip().split("\n")
        except:
            hits = []
        if hits != []:
            ucscName = page.split("/")[2]
            hits = str(hits)
            assemblyAlias[ucscName] = {}
            try:
                accession = regex.findall(hits)[0]
                if accession.startswith("GCA"):
                    assemblyAlias[ucscName]['GCA'] = accession
                elif accession.startswith("GCF"):
                    assemblyAlias[ucscName]['GCF'] = accession
            except:
                print("The following page could not be matched with the accession format",page,hits)
    return(assemblyAlias)

def buildGCFdicFromGenbankFile(evaSnpVersionNumber):
    checkDirMakeDir("/hive/data/outside/eva"+evaSnpVersionNumber+"/")
    genbankSummaryDic = {}
    regexGenbankGCA = re.compile(r'GCA_[0-9]*\.[0-9]*')
    regexGenbankGCF = re.compile(r'GCF_[0-9]*\.[0-9]*')
    wgetFile("https://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_genbank.txt", "/hive/data/outside/eva"+evaSnpVersionNumber+"/")
    summaryFile = open('/hive/data/outside/eva'+evaSnpVersionNumber+'/assembly_summary_genbank.txt','r')
    for line in summaryFile:
        if "GCF_" in line:
            GCAacc = regexGenbankGCA.findall(line)[0]
            GCFacc = regexGenbankGCF.findall(line)[0]
            genbankSummaryDic[GCFacc] = GCAacc
    summaryFile.close()
    return(genbankSummaryDic)

def findGCAaccForUCSCgcfDbs(assemblyAlias,genbankSummaryDic):
    """Try to find matching GCA accession to UCSC dbs based on genbank file or curling web page"""
    n=0
    regexGenbankGCA = re.compile(r'GCA_[0-9]*\.[0-9]*')
    for key, value in assemblyAlias.items():
        if 'GCF' in value:
            if value['GCF'] in genbankSummaryDic.keys():
                assemblyAlias[key]['GCA'] = genbankSummaryDic[value['GCF']]
                n+=1
            else: #Try to find it online at NCBI
                ncbiGCF = value['GCF']
                ncbiWgetPage = bash("curl -L https://www.ncbi.nlm.nih.gov/assembly/"+ncbiGCF+"/").rstrip().split("\n")
                for line in ncbiWgetPage:
                    if "GenBank assembly accession" in line and "GCA_" in line:
                        for subLine in line.split("/dd"):
                            if "GenBank assembly accession" in subLine:
                                GCAacc = regexGenbankGCA.findall(subLine)[0]
                                assemblyAlias[key]['GCA'] = GCAacc
                                n+=1
                    elif "ncbi_uidlist" in line and "GCA_" in line:
                        GCAacc = regexGenbankGCA.findall(line)[0]
                        assemblyAlias[key]['GCA'] = GCAacc
                        n+=1
    print("Total number of UCSC GCFs matched with GCAs: ",n)
    return(assemblyAlias)

def sanitizeListToOnlyUCSCdbsWithGCA(assemblyAlias):
    n = 0
    m=0
    ucscGCAlist = {}
    for key, value in assemblyAlias.items():
            if 'GCA' in value:
                if value['GCA'] != 'GCA_000':
                    ucscGCAlist[key] = value['GCA']
                n+=1
            else:
                m+=1
    print("Total number of UCSC databases already with a GCA:",n)
    print("Total number of UCSC databases with GCF that will need GCA lookup:",m)
    return(ucscGCAlist)

def buildFinalDicOfUCSCdbsWithEvaSnpData(ucscGCAlist,evaSnpVersionNumber):
    ucscEvaSnpDbs = {}
    n=0
    regexEvaSnpGCA = re.compile(r'GCA_[0-9]*\.[0-9]*')
    evaReleasePage = bash("curl https://ftp.ebi.ac.uk/pub/databases/eva/rs_releases/release_"+evaSnpVersionNumber+"/by_assembly/").rstrip().split("\n")
    activeRRassemblies = bash("""hgsql -h genome-centdb -e "select name from dbDb where active='1'" hgcentral""")
    for key in ucscGCAlist:
        if ucscGCAlist[key] in str(evaReleasePage):
            for line in evaReleasePage:
                if ucscGCAlist[key] in line:
                    if key in activeRRassemblies:
                        fullGCA = regexEvaSnpGCA.findall(line)[0]
                        ucscEvaSnpDbs[key] = fullGCA
                        n+=1
    ucscEvaSnpDbs['mm10']='GCA_000001635.6' #Manually add mm10 which is weird due to patches
    print("Total number of assemblies with EvaSnp data: ",n)
    return(ucscEvaSnpDbs)

def checkDirMakeDir(workDir):
    if not os.path.isdir(workDir):
        os.mkdir(workDir)

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

def wgetFile(url, workDir):
    bash("wget -N %s -P %s" % (url, workDir))
    fileName = workDir+url.split("/")[-1]
    return(fileName)

def checkDupsAndUnzip(fileName,workDir,ucscDatabaseName):
    """Check for duplicate lines (common in EVA), and unzip file"""
    lineCount = bash("zcat %s | wc -l" % (fileName)).rstrip()
    uniqCount = bash("zcat %s | uniq | wc -l" % (fileName)).rstrip()
    currentFileName = workDir+"evaSnps.vcf"
    #Add catches below in places where the chrom names don't match or are missing from UCSC for specific dbs
    if ucscDatabaseName == 'galGal5':
        bash("zcat %s | uniq | grep -v 'X52392.1' > %s" % (fileName,currentFileName))
    elif ucscDatabaseName == 'bosTau9':
        bash("zcat %s | uniq | grep -v 'MT\|AY526085.1' > %s" % (fileName,currentFileName)) #Added AY526085.1 because that is our chrMT but eva SNP mislabeled it something else
    else:
        if lineCount != uniqCount:
            print("There are duplicated entries.\nOriginal file:%s\nUniq file:%s\nCreating new file." % (lineCount, uniqCount))
            bash("zcat %s | uniq > %s" % (fileName,currentFileName))
        else:
            bash("zcat %s > %s" % (fileName,currentFileName))
    return(currentFileName)

def sanitizeChromNames(currentFileName,dbs,workDir):
    """Swaps the chrom IDs with the accession names which are indexed in chromAlias"""
    inputFile = open(currentFileName,'r')
    outputFile = open(currentFileName+".fixed","w")
    IDtoAccessionDic = {}

    for line in inputFile:
        if line.startswith("##contig"):
            ID = line.split('=')[2].split(',')[0]
            accession = line.split('=')[3].split('"')[1]
            IDtoAccessionDic[ID] = accession
            outputFile.write(line)
        elif line.startswith("#"):
            outputFile.write(line)
        else:
            line = line.split("\t")
            line[0] = IDtoAccessionDic[line[0]]
            line = "\t".join(line)
            outputFile.write(line)
    inputFile.close()
    outputFile.close()
    #Replacing the same name because chromAlias should be updated soon and this function will be removed
    bash("mv %s.fixed %s" % (currentFileName,currentFileName))
    currentFileName = convertChromToUcsc(currentFileName,dbs,workDir)
    print("Chroms converted")

def addChrToChromNumbers(fileName,currentFileName,workDir):
    bash("zcat %s | uniq > %s" % (fileName,currentFileName))
    with open(workDir+"evaSnps.ucscChroms.vcf",'w') as newChromVcf:
        with open(currentFileName,"r") as oldChromVcf:
            for line in oldChromVcf:
                if line.startswith("#"):
                    newChromVcf.write(line)
                elif line.startswith("AC024175.3"): #Exception for zebrafish
                    newChromVcf.write(line.replace("AC024175.3","chrM"))
                elif line.startswith("AY612638.1"): #Exception for rheMac8
                    newChromVcf.write(line.replace("AY612638.1","chrM"))
                elif line.startswith("KP263414.1"): #Exception for sacCer3
                    newChromVcf.write(line.replace("KP263414.1","chrM"))
                elif line.startswith("X54252.1"): #Exception for ce11
                    newChromVcf.write(line.replace("X54252.1","chrM"))
                else:
                    newChromVcf.write("chr"+line)
    currentFileName = workDir+"evaSnps.ucscChroms.vcf"
    return(currentFileName)

def convertChromToUcsc(currentFileName,dbs,workDir):
    """Convert to UCSC-style chroms"""
    bash("chromToUcsc --get %s" % (dbs))
    bash("chromToUcsc -i %s -o %sevaSnps.ucscChroms.vcf -a %s" % (currentFileName,workDir,dbs+".chromAlias.tsv"))
    bash("rm %s" % (dbs+".chromAlias.tsv"))
    currentFileName = workDir+"evaSnps.ucscChroms.vcf"
    return(currentFileName)

def convertVcfToBed(currentFileName,workDir):
    """Convert to bed keeping only VC and SID fields"""
    bash("/cluster/bin/x86_64/bgzip %s" % (currentFileName))
    bash("/cluster/bin/x86_64/tabix -p vcf %s" % (currentFileName+".gz"))
    bash("vcfToBed %s %sevaSnp -fields=VC,SID" % (currentFileName+".gz",workDir))
    currentFileName = workDir+"evaSnp.bed"
    return(currentFileName)

def buildChromSizesDic(workDir,dbs):
    """Create a dictionary the chromSizes"""
    chromSizes = bash("fetchChromSizes %s" % (dbs)).split("\n")
    chromDic = {}
    for chrom in chromSizes[1:-1]:
        chrom = chrom.split("\t")
        chromDic[chrom[0]] = chrom[1]
    return(chromDic)

def splitChromsAndRunHgVai(workDir,dbs):
    """Split all the chroms in tenths in order to be able to run hgVai without running out of memory"""
    chromSizes = bash("fetchChromSizes %s" % (dbs)).split("\n")
    inputFile = workDir+"evaSnps.ucscChroms.vcf.gz"
    outputFile = workDir+"evaSnps"+dbs+"VaiResults.vep"
    n=0
    allTables = bash("hgsql -e \"show tables\" "+dbs+"")
    if dbs in ['galGal6','oviAri4','ponAbe3', 'danRer7','bisBis1','bosTau6','bosTau9']: #Special exception to use refGene because ncbiRefSeq has incorrect protein sequence in Link table see #29262. For danRer7/bisBis1/bosTau6 ensGene has a bad exon see #34135.
        geneTableToUse = "refGene"
#     elif "ncbiRefSeqSelect" in allTables: #Removing refseq select as the annotations are too sparse
#         geneTableToUse = "ncbiRefSeqSelect"
    elif "ncbiRefSeqCurated" in allTables:
        geneTableToUse = "ncbiRefSeqCurated"
    elif "ensGene" in allTables:
        geneTableToUse = "ensGene"
    elif "refGene" in allTables:
        geneTableToUse = "refGene"
    elif "ncbiGene" in allTables:
        geneTableToUse = "ncbiGene"
    else:
        print(dbs)
        sys.exit("Could not find any tables to use for the following database: "+dbs)

    chromDic = buildChromSizesDic(workDir,dbs)
    #For function below, only bother with the chromosomes in the VCF
    chromsInVcf = bash("zcat "+inputFile+" | grep -v '^#' | cut -f1 | uniq").rstrip().split("\n")

    for chrom in chromsInVcf:
        chromName = chrom
        chromSize = chromDic[chrom]
        n+=1
        if (n % 100) == 0:
            print("Have currently run "+str(n)+"/"+str(len(chromsInVcf))+" chroms. Currently running "+chromName)
        if int(chromSize) < 1000000: #Don't worry about splitting if chrom is less than 1M bp
            bash("vai.pl --variantLimit=200000000 --hgVai=/usr/local/apache/cgi-bin/hgVai --position=%s:0-%s --geneTrack=%s --hgvsG=off --hgvsCN=off \
            --hgvsP=on %s %s >> %s" % (chromName,chromSize,geneTableToUse,dbs,inputFile,outputFile))
        else:
            breakPoints = [.2,.4,.6,.8,1]
            for part in breakPoints:
                startCoord = round(int(chromSize)*(part-.2))
                endCoord = round(int(chromSize)*part)
                if part == breakPoints[0]: #Special exception for start
                    bash("vai.pl --variantLimit=200000000 --hgVai=/usr/local/apache/cgi-bin/hgVai --position=%s:0-%s --geneTrack=%s --hgvsG=off --hgvsCN=off \
                    --hgvsP=on %s %s >> %s" % (chromName,endCoord,geneTableToUse,dbs,inputFile,outputFile))
                else:
                    bash("vai.pl --variantLimit=200000000 --hgVai=/usr/local/apache/cgi-bin/hgVai --position=%s:%s-%s --geneTrack=%s --hgvsG=off --hgvsCN=off \
                    --hgvsP=on %s %s >> %s" % (chromName,startCoord,endCoord,geneTableToUse,dbs,inputFile,outputFile))
    return(outputFile)

def splitVepFileByChrom(hgVaiResultsFile,workDir):
    """Split the vep file to one file per chrom in order to index entire chrom into a dic and quickly lookup the rsIDs"""
    splitChromDir = workDir+"splitChroms/"
    checkDirMakeDir(splitChromDir)
    vepFile = open(hgVaiResultsFile,"r")
    n=0
    for line in vepFile:
        if line.startswith("#") or line.startswith("Uploaded"):
            continue
        elif line.startswith("\n") or line.startswith("Content") or line.startswith("Error") or line.startswith("Location") or line.startswith("vpExpandIndelGaps") or line.startswith("needMem"):
            n+=1
            continue
        else:
            #Need to compute itemRGB, aaChange, and ucscClass
            lineContents = line.rstrip().split("\t")
            chrom = lineContents[1].split(":")[0]
            outFile = open(splitChromDir+"evaSnpVaiResults."+chrom+".vep","a")
            outFile.write(line)
            outFile.close()
    vepFile.close()
    if n != 0:
        print(str(n)+" number of bad entries were found in the .vep file. This typically occurs from bad ncbi protein alignments that error out the VEP. If you see more than 20-30 that is bad.")

def fetchUcscClassAndAaChange(name,alt,nameAlt,chrom,currentChrom,currentVepDic,tryAttempts,badTryAttempts,workDir):
    """Build dictionary out of vep results with rsID as index, populate aaChange and ucscClassList"""
    prevName = nameAlt
    #Check if chrom has changed and index current chrom
    if chrom != currentChrom:
        currentVepFile = open(workDir+"splitChroms/evaSnpVaiResults."+chrom+".vep","r")
        currentVepDic = {}
        for entry in currentVepFile:
            entry = entry.rstrip().split("\t")
            rsID = entry[0]
            rsIDAlt = entry[0]+entry[2]
            if rsIDAlt not in currentVepDic.keys():
                currentVepDic[rsIDAlt] = {}
                currentVepDic[rsIDAlt]['ucscClassList'] = []
                currentVepDic[rsIDAlt]['ucscClassList'].append(entry[6])
                currentVepDic[rsIDAlt]['aaChange'] = []
                if entry[13] != '-' and 'p' in entry[13] and '?' not in entry[13]:
                    currentVepDic[rsIDAlt]['aaChange'].append(entry[13].split(";")[0].split("=")[1].split(":")[1])
            else:
                if entry[6] not in currentVepDic[rsIDAlt]['ucscClassList']:
                    currentVepDic[rsIDAlt]['ucscClassList'].append(entry[6])
                if entry[13] != '-' and 'p' in entry[13] and '?' not in entry[13]:
                    if entry[13].split(";")[0].split("=")[1] not in currentVepDic[rsIDAlt]['aaChange']:
                        currentVepDic[rsIDAlt]['aaChange'].append(entry[13].split(";")[0].split("=")[1].split(":")[1])

            if rsID not in currentVepDic.keys():
                currentVepDic[rsID] = {}
                currentVepDic[rsID]['ucscClassList'] = []
                currentVepDic[rsID]['ucscClassList'].append(entry[6])
                currentVepDic[rsID]['aaChange'] = []
                if entry[13] != '-' and 'p' in entry[13] and '?' not in entry[13]:
                    currentVepDic[rsID]['aaChange'].append(entry[13].split(";")[0].split("=")[1].split(":")[1])
            else:
                if entry[6] not in currentVepDic[rsID]['ucscClassList']:
                    currentVepDic[rsID]['ucscClassList'].append(entry[6])
                if entry[13] != '-' and 'p' in entry[13] and '?' not in entry[13]:
                    if entry[13].split(";")[0].split("=")[1] not in currentVepDic[rsID]['aaChange']:
                        currentVepDic[rsID]['aaChange'].append(entry[13].split(";")[0].split("=")[1].split(":")[1])

        currentChrom=chrom
        currentVepFile.close()

    try:
        ucscClass = ",".join(currentVepDic[nameAlt]['ucscClassList'])
        aaChange = " ".join(currentVepDic[nameAlt]['aaChange'])
    except:
        try:
            tryAttempts+=1
            ucscClass = ",".join(currentVepDic[name]['ucscClassList'])
            aaChange = " ".join(currentVepDic[name]['aaChange'])
        except: #This should only kick in when there are a few bad vars that VAI can't process
            badTryAttempts+=1
            ucscClass = "NULL"
            aaChange = "NULL"

    return(prevName,currentChrom,currentVepDic,ucscClass,aaChange,tryAttempts,badTryAttempts)

def assignRGBcolorByConsequence(currentVepDic,name,alt,nameAlt):
    """Assign color based on most serious hgVai predicted consequence"""

    redVars = ['exon_loss_variant','frameshift_variant','inframe_deletion','inframe_insertion','initiator_codon_variant',
               'missense_variant','splice_acceptor_variant','splice_donor_variant','splice_region_variant','stop_gained',
               'stop_lost','coding_sequence_variant','transcript_ablation']
    greenVars = ['synonymous_variant','stop_retained_variant']
    blueVars = ['5_prime_UTR_variant','3_prime_UTR_variant','complex_transcript_variant',
                'non_coding_transcript_exon_variant']
    blackVars = ['upstream_gene_variant','downstream_gene_variant','intron_variant','intergenic_variant',
                 'NMD_transcript_variant','no_sequence_alteration']

    try:
        if any(item in currentVepDic[nameAlt]['ucscClassList'] for item in redVars):
            itemRgb = '255,0,0'
        elif any(item in currentVepDic[nameAlt]['ucscClassList'] for item in greenVars):
            itemRgb = '0,128,0'
        elif any(item in currentVepDic[nameAlt]['ucscClassList'] for item in blueVars):
            itemRgb = '0,0,255'
        elif any(item in currentVepDic[nameAlt]['ucscClassList'] for item in blackVars):
            itemRgb = '0,0,0'
        else:
            sys.exit("One of the following consequences not found in color list, please add: "+str(currentVepDic[name]['ucscClassList']))
    except:
        if any(item in currentVepDic[name]['ucscClassList'] for item in redVars):
            itemRgb = '255,0,0'
        elif any(item in currentVepDic[name]['ucscClassList'] for item in greenVars):
            itemRgb = '0,128,0'
        elif any(item in currentVepDic[name]['ucscClassList'] for item in blueVars):
            itemRgb = '0,0,255'
        elif any(item in currentVepDic[name]['ucscClassList'] for item in blackVars):
            itemRgb = '0,0,0'
        else:
            sys.exit("One of the following consequences not found in color list, please add: "+str(currentVepDic[name]['ucscClassList']))


    return(itemRgb)

def convertSOnumberToName(varClass):
    soDic={"SO:0001483": 'substitution', "SO:0000667": 'insertion', "SO:0000159": 'deletion',
           "SO:0001059": 'sequence alteration',"SO:1000032": 'delins',"SO:0002007": 'MNV', "SO:0000705": 'tandem_repeat'}
    varClass = soDic[varClass]
    return(varClass)

def buildDuplicateRsIDset(currentFileName):
    """Look for multi-allelic variants to then try and more accurately match those hgVai results"""
    duplicateSet = set()
    inputFile = open(currentFileName,"r")
    prevName = False
    for line in inputFile:
        if line.startswith("#") or line.startswith("Uploaded"):
            continue
        else:
            lineContents = line.rstrip().split("\t")
            name = lineContents[3]
            if prevName == name:
                duplicateSet.add(name)
            prevName = name
    print("Total number of multi-allelic variants: "+str(len(duplicateSet)))
    return(duplicateSet)

def createFinalBedWithVepAnnotations(currentFileName,workDir):
    """Parse bed file and output final file without FILTER field, and with SO name, item RGB, and aaChange/ucscClass from hgVai"""

    inputFile = open(currentFileName,"r")
    outputFile = open(workDir+"evaSnp.final.bed","w")
    prevName = ""
    currentChrom = ""
    currentVepDic = {}
    n=0
    tryAttempts=0
    badTryAttempts=0
    duplicateSet = buildDuplicateRsIDset(currentFileName)

    for line in inputFile:
        n+=1
        if n%5000000 == 0:
            print("Current line count: "+str(n))
        if line.startswith("#") or line.startswith("Uploaded"):
            continue
        else:
        #Desired schema chrom chromStart chromEnd name score strand thickStart thickEnd itemRgb ref alt varClass submitters ucscClass aaChange
        #Need to compute itemRGB, aaChange, and ucscClass. Also need to change SO terms

            lineContents = line.rstrip().split("\t")
            chrom = lineContents[0]
            chromStart = lineContents[1]
            chromEnd = lineContents[2]
            name = lineContents[3]
            ref = lineContents[9]
            alt = lineContents[10]
            varClass = lineContents[12]
            submitters = lineContents[13]

            #Convert from SO term to name. e.g. SO:0001483 to substitution
            varClass = convertSOnumberToName(varClass)

            #Check if is the same rsID, they are repeated in ~5% of file
            if prevName == name:
                print("Repeated prevName: "+prevName)
                outputFile.write(chrom+"\t"+chromStart+"\t"+chromEnd+"\t"+name+"\t0\t.\t"+chromStart+"\t"+chromEnd+"\t"+
                                 itemRgb+"\t"+ref+"\t"+alt+"\t"+varClass+"\t"+submitters+"\t"+ucscClass+"\t"+aaChange+"\n")
            elif name in duplicateSet:
                #Find the aaChange and ucscClass
                #Try and match the allele with what hgVai outputs
                if varClass == "insertion":
                    if ',' in alt:
                        nameAlt = name+alt.split(",")[0][1:]
                    elif len(alt)-1 >= 59:
                        nameAlt = name+alt[len(ref):25]+"..."+alt[len(alt)-24:]+"("+str(len(alt)-1)+" nt)"
                    else:
                        nameAlt = name+alt[1:]
                elif varClass == "deletion":
                    nameAlt = name+"-"
                elif varClass == "tandem_repeat":
                    if len(alt) > len(ref):
                        nameAlt = name+alt[len(ref):]
                    else:
                        nameAlt = name+"-"
                else:
                    nameAlt = name+alt

                
                prevName,currentChrom,currentVepDic,ucscClass,aaChange,tryAttempts,badTryAttempts = fetchUcscClassAndAaChange(name,alt,nameAlt,chrom,currentChrom,currentVepDic,tryAttempts,badTryAttempts,workDir)
                #Check for bad variants VAI can't process
                if ucscClass == "NULL":
                    continue
                else:
                    itemRgb = assignRGBcolorByConsequence(currentVepDic,name,alt,nameAlt)
                    outputFile.write(chrom+"\t"+chromStart+"\t"+chromEnd+"\t"+name+"\t0\t.\t"+chromStart+"\t"+chromEnd+"\t"+
                                 itemRgb+"\t"+ref+"\t"+alt+"\t"+varClass+"\t"+submitters+"\t"+ucscClass+"\t"+aaChange+"\n")
            else:
                nameAlt = name
                prevName,currentChrom,currentVepDic,ucscClass,aaChange,tryAttempts,badTryAttempts = fetchUcscClassAndAaChange(name,alt,nameAlt,chrom,currentChrom,currentVepDic,tryAttempts,badTryAttempts,workDir)
                #Check for bad variants VAI can't process
                if ucscClass == "NULL":
                    continue
                else:
                    itemRgb = assignRGBcolorByConsequence(currentVepDic,name,alt,nameAlt)
                    outputFile.write(chrom+"\t"+chromStart+"\t"+chromEnd+"\t"+name+"\t0\t.\t"+chromStart+"\t"+chromEnd+"\t"+
                                 itemRgb+"\t"+ref+"\t"+alt+"\t"+varClass+"\t"+submitters+"\t"+ucscClass+"\t"+aaChange+"\n")

    print("Total number of try attempts, this number should be <100 = "+str(tryAttempts))
    print("Total number of times an rsID was missing from vep result, this typically happens when errors happened \
          runing hgVai that were skipped over. Ideally less than 1000: " + str(badTryAttempts))
    print("Total number of lines in output file = "+str(n))
    inputFile.close()
    outputFile.close()

def validateFinalFile(workDir,url,dbs):
    """Compare and make sure that the final bed rsID# matches the input file"""
    originalDownloadFileName = workDir+url.split("/")[-1]
    bedFile = workDir+"evaSnp.final.bed"
    originalRsIDCount = bash("zcat %s | grep -v '^#' | cut -f3 | sort | uniq | wc -l" % (originalDownloadFileName))
    finalBedRsIDCount = bash("cut -f4 %s | sort | uniq | wc -l" % (bedFile))
    #Skip the check for galGal5 which is known to have troublesome chrMT name
    if dbs != "galGal5" and dbs != "bosTau9" and dbs != 'ce11':
        if originalRsIDCount != finalBedRsIDCount:
            sys.exit("There was an error in the pipeline, the initial number of uniq RS IDs does not match the final bed file.\n\n"\
    "%s - %s%s - %s" % (originalDownloadFileName,str(originalRsIDCount),bedFile,str(finalBedRsIDCount)))
    finalBedLineCount = bash("cat %s | wc -l" % (bedFile))
    finalBedUniqueLineCount = bash("cat %s | sort | uniq | wc -l" % (bedFile))
    if finalBedLineCount != finalBedUniqueLineCount:
        bash("sort -u -o %s %s" % (bedFile,bedFile))
        print("There are duplicate entries in the file: "+str(finalBedLineCount)+" vs "+str(finalBedUniqueLineCount))
        print("This could be troublesome depending on the number of duplicates. The final file was uniq'd and created.")

def createBigBed(workDir,dbs,evaSnpVersionNumber):
    bedFile = workDir+"evaSnp.final.bed"
    bash("bedSort %s %s" % (bedFile,bedFile))
    bash("bedToBigBed -tab -as=$HOME/kent/src/hg/lib/evaSnp.as -type=bed9+6 -extraIndex=name \
    %s http://hgdownload.soe.ucsc.edu/goldenPath/%s/bigZips/%s.chrom.sizes %sevaSnp%s.bb" % (bedFile,dbs,dbs,workDir,evaSnpVersionNumber))

def checkIfDbsDoneOrToSkip(key,evaSnpVersionNumber):
    dbsToSkip = ['wuhCor1', 'canFam4', 'canFam6', 'mm9'] #List of databases with known issues on evaSnp - canFam4/6 and mm9 are empty dirs, they said they don't annotate them
    if os.path.exists("/hive/data/outside/eva"+evaSnpVersionNumber+"/"+key+"/evaSnp"+evaSnpVersionNumber+".bb"):
        print("DBS already completed: "+key)
        return(True)
    elif key in dbsToSkip:
        print("This assembly is on the skip list. Skipping: "+key)
        return(True)
    else:
        if os.path.exists("/hive/data/outside/eva"+evaSnpVersionNumber+"/"+key+"/evaSnps.ucscChroms.vcf") or os.path.exists("/hive/data/outside/eva"+evaSnpVersionNumber+"/"+key+"/evaSnps.ucscChroms.vcf.gz"):
            print("Cleaning up directory for previous attempt of "+key)
            bash("rm /hive/data/outside/eva"+evaSnpVersionNumber+"/"+key+"/evaSnps*")
            if os.path.exists("/hive/data/outside/eva"+evaSnpVersionNumber+"/"+key+"/splitChroms"):
                try:
                    bash("rm /hive/data/outside/eva"+evaSnpVersionNumber+"/"+key+"/splitChroms/*")
                except:
                    print("Directly cleaned")
        return(False)
    
def findFileUrl(GCAacc,evaSnpVersionNumber,dbs):
    dirStructure = bash("curl https://ftp.ebi.ac.uk/pub/databases/eva/rs_releases/release_"+evaSnpVersionNumber+"/by_assembly/"+GCAacc+"/ | grep '[DIR]'")
    #Loops to pick correct species name files since Ensembl began to add multiple species per GCA in eva5
    if dbs == 'oviAri3':
        speciesName = 'ovis_orientalis/'
    elif dbs == 'bosTau6':
        speciesName = 'bos_indicus_x_bos_taurus/'
    elif dbs == 'bosTau9':
        speciesName = 'bos_taurus/'
    else:
        speciesName = dirStructure.split("href")[6].split('"')[1]
    urlToFileDir = "https://ftp.ebi.ac.uk/pub/databases/eva/rs_releases/release_"+evaSnpVersionNumber+"/by_assembly/"+GCAacc+"/"+speciesName
    fileDir = bash("curl -L "+urlToFileDir+" | grep "+GCAacc)
    filespeciesNameDirHref = fileDir.split("href")
    fileName = filespeciesNameDirHref[2].split('"')[1]
    urlToFile = urlToFileDir+fileName
    return(urlToFile)

def buildEvaSnpTrack(ucscDatabaseName,ucscEvaSnpDbs,evaSnpVersionNumber):
    """Build evaSnp track from start to finish"""
    dbs = ucscDatabaseName
    GCAacc = ucscEvaSnpDbs[dbs]
    workDir = "/hive/data/outside/eva"+evaSnpVersionNumber+"/"+dbs+"/"
    urlToFile = findFileUrl(GCAacc,evaSnpVersionNumber,dbs)
    checkDirMakeDir(workDir)
    fileName = wgetFile(urlToFile, workDir)
    fileName = workDir+urlToFile.split("/")[-1]
    print("Got file: ",dbs)
    currentFileName = checkDupsAndUnzip(fileName,workDir,ucscDatabaseName)
    print("File unzipped")
    try:
        currentFileName = convertChromToUcsc(currentFileName,dbs,workDir)
        print("Chroms converted")
    except:
        try:
            sanitizeChromNames(currentFileName,dbs,workDir)
            print("Chroms sanitized")
            currentFileName = convertChromToUcsc(currentFileName,dbs,workDir)
            print("Chroms converted")
        except:
            currentFileName = addChrToChromNumbers(fileName,currentFileName,workDir)
            print("Chroms completed after adding '1' to starts.")
    currentFileName = convertVcfToBed(currentFileName,workDir)
    print("File converted to bed")
    hgVaiResultsFile = splitChromsAndRunHgVai(workDir,dbs)
    print("hgVai done")
    splitVepFileByChrom(hgVaiResultsFile,workDir)
    print("hgVai file split")
    currentFileName = workDir+"evaSnp.bed"
    createFinalBedWithVepAnnotations(currentFileName,workDir)
    print("Validating file")
    validateFinalFile(workDir,urlToFile,dbs)
    print("Created final file")
    createBigBed(workDir,dbs,evaSnpVersionNumber)
    print("bigBed made")
    
def main(mode):
    #Run main with either "all" or a specific UCSC database name
    #Find all the assemblies we have with matching evaSnp data
    evaSnpVersionNumber = "8"
    assemblyAlias = findAllAccFromDescPages()
    genbankSummaryDic = buildGCFdicFromGenbankFile(evaSnpVersionNumber)
    assemblyAlias = findGCAaccForUCSCgcfDbs(assemblyAlias,genbankSummaryDic)
    ucscGCAlist = sanitizeListToOnlyUCSCdbsWithGCA(assemblyAlias)
    ucscEvaSnpDbs = buildFinalDicOfUCSCdbsWithEvaSnpData(ucscGCAlist,evaSnpVersionNumber)
    #Build evaSNP data for either one specific assembly, or all assemblies
    if mode == 'all':
        for key in ucscEvaSnpDbs:
            if not checkIfDbsDoneOrToSkip(key,evaSnpVersionNumber):
                try:
                    buildEvaSnpTrack(key,ucscEvaSnpDbs,evaSnpVersionNumber)
                except:
                    print("Problem with DBS: "+key)
                    continue
    else:
        if not checkIfDbsDoneOrToSkip(mode,evaSnpVersionNumber):
            buildEvaSnpTrack(mode,ucscEvaSnpDbs,evaSnpVersionNumber)

main("all")
