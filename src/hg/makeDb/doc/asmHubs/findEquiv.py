#!/usr/bin/python3

# given some list of accessions: GCx_012345678.9
# check to see if they are in GenArk or they at least
# have GCF/GCA equivalents in GenArk.
#
# arguments are: the file list of accessions
#           and: file to write resulting two column tsv list:
#           accessionFromInputList <tab> GenArkIdentifier
# where the GenArkIdentifier will be the full name:
#           GCx_012345678,9_asmName

import csv
import sys
import subprocess

#############################################################################
### read the genark list: "UCSC_GI.assemblyHubList.txt"
### obtained via rsync: 
### rsync -a -P \
###   rsync://hgdownload.soe.ucsc.edu/hubs/UCSC_GI.assemblyHubList.txt ./
#############################################################################
def readGenArk(hubList):
    genArk = {}
    noHeaders = ["grep", "-a", "-v", "^#", hubList]
    cutCommand = ["cut", "-f1,2"]
    onlyGc = ["grep", "^GC"]
    sortCommand = ["sort", "-u"]
    try:
        noHeaderProc = subprocess.Popen(noHeaders, stdout=subprocess.PIPE)
        cutProc = subprocess.Popen(cutCommand, stdin=noHeaderProc.stdout, stdout=subprocess.PIPE)
        matchGc = subprocess.Popen(onlyGc, stdin=cutProc.stdout, stdout=subprocess.PIPE)
        sortProcess = subprocess.Popen(sortCommand, stdin=matchGc.stdout, stdout=subprocess.PIPE, text=True)
        
        for line in sortProcess.stdout:
            parts = line.strip().split('\t')
            if len(parts) == 2:
                key, asmName = parts
                genArk[key] = asmName
    except Exception as e:
        sys.stderr.write(f"Error processing grep pipeline: {e}\n")
        sys.exit(1)
    
    print(f"### len(genArk): {len(genArk)}")
    return genArk

#############################################################################
### read in the identifier list to check
#############################################################################
def readIdentifiers(filePath):
    identifierDict = {}
    try:
        with open(filePath, 'r') as file:
            for line in file:
                identifier = line.strip()
                if identifier:
                    identifierDict[identifier] = True
    except Exception as e:
        sys.stderr.write(f"Error reading file {filePath}: {e}\n")
        sys.exit(1)
    return identifierDict

#############################################################################
### read the NCBI assembly_summary files for all RefSeq, current and historical
### return an equivalent GCF->GCA dictionary with identical/different status
#############################################################################
def readGrepData():
    file1 = "/hive/data/outside/ncbi/genomes/reports/assembly_summary_refseq.txt"
    file2 = "/hive/data/outside/ncbi/genomes/reports/assembly_summary_refseq_historical.txt"
    noHeaders = ["grep", "-h", "-v", "^#", file1, file2]
    onlyGcf = ["grep", "GCF_"]
    andGca = ["grep", "GCA_"]
    cutCommand = ["cut", "-f1,18,19"]
    sortCommand = ["sort", "-u"]
    
    dataDict = {}
    try:
        noHeaderProc = subprocess.Popen(noHeaders, stdout=subprocess.PIPE)
        gcfProc = subprocess.Popen(onlyGcf, stdin=noHeaderProc.stdout, stdout=subprocess.PIPE)
        andGcaProc = subprocess.Popen(andGca, stdin=gcfProc.stdout, stdout=subprocess.PIPE)
        cutProcess = subprocess.Popen(cutCommand, stdin=andGcaProc.stdout, stdout=subprocess.PIPE)
        sortProcess = subprocess.Popen(sortCommand, stdin=cutProcess.stdout, stdout=subprocess.PIPE, text=True)
        
        for line in sortProcess.stdout:
            parts = line.strip().split('\t')
            if len(parts) == 3:
                key, col18, col19 = parts
                dataDict[key] = (col18, col19)
                dataDict[col18] = (key, col19)
    except Exception as e:
        sys.stderr.write(f"Error processing grep pipeline: {e}\n")
        sys.exit(1)
    
    print(f"### len(gcaGcf): {len(dataDict)}")
    return dataDict

#############################################################################
### now with all lists in dictionaries, check the equivalence
### return an identity equivalent dictionary with the key as the
### original identifier, and the value the GenArk identifier
#############################################################################
def relateData(identifierDict, genArk, dataDict):
    identEquiv = {}

    initialIdentCount = len(identifierDict)
    print(f"### checking {initialIdentCount} identifiers")

    genArkMatch = identifierDict.keys() & genArk.keys()
    for key in genArkMatch:
        identEquiv[key] = f"{key}_{genArk[key]}"

    genArkCount = len(identEquiv)
    print(f"### genArk perfect matched {genArkCount}")
    notMatched = identifierDict.keys() - identEquiv.keys()
    print(f"### remaining identifiers to match {len(notMatched)}")

    equivCount = 0
    # check for equivalent GCA/GCF in genArk
    for key in notMatched:
        if key in dataDict:
            equivKey = dataDict[key][0]
            if equivKey in genArk:
                if equivKey not in identEquiv:
                    identEquiv[key] = f"{equivKey}_{genArk[equivKey]}"
                    equivCount += 1
                    print(f"{key}\t{identEquiv[key]}")

    print(f"### found an additional {equivCount} equivalents")

    return identEquiv

#############################################################################
### main()
#############################################################################
if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.stderr.write("Usage: findEquiv.py <identifierFile> <resultFile> <notFoundFile>\n")
        sys.exit(1)
    
    identifierFile = sys.argv[1]
    resultFile = sys.argv[2]
    notFoundFile = sys.argv[3]
    identifierDict = readIdentifiers(identifierFile)
    genArkDict = readGenArk("UCSC_GI.assemblyHubList.txt")
    dataDict = readGrepData()
    identEquiv = relateData(identifierDict, genArkDict, dataDict)

    # write results, sorted
    with open(resultFile, "w", newline="\n") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        for key, value in sorted(identEquiv.items()):
            writer.writerow([key, value])

    # check if all items matched, if not write them out to notFoundFile
    notFound = [key for key in identifierDict if key not in identEquiv]
    if len(notFound) > 0:
        print(f"### {len(notFound)} identifiers not found written to: {notFoundFile}")
        with open(notFoundFile, "w", newline="\n") as file:
            file.writelines(f"{key}\n" for key in sorted(notFound))
    else:
        with open(notFoundFile, "w", newline="\n") as file:
            file.write(f"# all matched OK\n")
