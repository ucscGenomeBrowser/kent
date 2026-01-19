#!/cluster/software/bin/python3

import subprocess
import locale
import sys
import re
import os
import csv
import requests
from io import StringIO

# priorities derived from the 'Monthly usage stats' report
# from the qateam cron job running on the first day of the month
# special top priorities
topPriorities = {
    'hg38': 1,
    'mm39': 2,
    'hs1': 3,
    'hg19': 4,
    'mm10': 5,
    'dm6': 6,
    'danRer11': 7,
    'mm9': 8,
    'hetGla2': 9,
    'rn6': 10,
    'hg18': 11,
    'galGal6': 12,
    'bosTau9': 13,
    'ce11': 14,
    'canFam4': 15,
}

### key will be dbDb/GCx name, value will be priority number
allPriorities = {}

priorityCounter = len(topPriorities) + 1

# key is clade, value is priority, will be initialized
# by initCladePriority() function
cladePrio = {}

####################################################################
### this is kinda like an environment setting, it gets everything
### into a UTF-8 reading mode
####################################################################
def set_utf8_encoding():
    """
    Set UTF-8 encoding for stdin, stdout, and stderr in Python.
    """
    if sys.stdout.encoding != 'utf-8':
        sys.stdout = open(sys.stdout.fileno(), mode='w', encoding='utf-8', buffering=1)
    if sys.stderr.encoding != 'utf-8':
        sys.stderr = open(sys.stderr.fileno(), mode='w', encoding='utf-8', buffering=1)

####################################################################
### reading hgcentral.dbDb table
####################################################################
def dbDbData():
    # Run the MySQL command and capture the output as bytes
    result = subprocess.run(
        ["hgsql", "-hgenome-centdb", "-N", "-e", "SELECT name,scientificName,organism,taxId,sourceName,description FROM dbDb WHERE active=1;", "hgcentral"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    if result.returncode != 0:
        print(f"Error executing MySQL command: {result.stderr.decode('utf-8')}")
        exit(1)
    # Decode the output from bytes to string using utf-8
    return result.stdout.decode('latin-1')

####################################################################
### this clade file is setup via weekly cron jobs
### 05 00 * * 3 /hive/data/outside/ncbi/genomes/cronUpdates/ncbiRsync.sh GCA
### 05 00 * * 6 /hive/data/outside/ncbi/genomes/cronUpdates/ncbiRsync.sh GCF
####################################################################
def readAsmIdClade():
    asmIdClade = {}

    filePath = "/hive/data/outside/ncbi/genomes/reports/newAsm/asmId.clade.tsv"

    with open(filePath, 'r', encoding='utf-8') as file:
        reader = csv.reader(file, delimiter='\t')
        for row in reader:
            asmIdClade[row[0]] = row[1]

    return asmIdClade

####################################################################
### this common name file is setup weekly via a cron job
####################################################################
def allCommonNames():
    commonNames = {}

    filePath = "/hive/data/outside/ncbi/genomes/reports/allCommonNames/asmId.commonName.all.txt"

    with open(filePath, 'r', encoding='utf-8') as file:
        reader = csv.reader(file, delimiter='\t')
        for row in reader:
            asmId = row[0]
            gcAcc = asmId.split('_')[0] + "_" + asmId.split('_')[1]
            commonNames[gcAcc] = row[1]

    return commonNames

####################################################################
"""
     header definitions from assembly_summary_refseq.txt

     1  #assembly_accession 20  ftp_path
     2  bioproject          21  excluded_from_refseq
     3  biosample           22  relation_to_type_material
     4  wgs_master          23  asm_not_live_date
     5  refseq_category     24  assembly_type
     6  taxid               25  group
     7  species_taxid       26  genome_size
     8  organism_name       27  genome_size_ungapped
     9  infraspecific_name  28  gc_percent
    10  isolate             29  replicon_count
    11  version_status      30  scaffold_count
    12  assembly_level      31  contig_count
    13  release_type        32  annotation_provider
    14  genome_rep          33  annotation_name
    15  seq_rel_date        34  annotation_date
    16  asm_name            35  total_gene_count
    17  asm_submitter       36  protein_coding_gene_count
    18  gbrs_paired_asm     37  non_coding_gene_count
    19  paired_asm_comp     38  pubmed_id

Would be good to verify this in the readAsmSummary to make sure it
hasn't changed.

   2175 archaea
 360585 bacteria
    607 fungi
    414 invertebrate
    184 plant
     96 protozoa
    231 vertebrate_mammalian
    405 vertebrate_other
  14992 viral


"""

####################################################################
### the various listings are ordered by these clade priorities to get
### primates first, mammals second, and so on
####################################################################
def initCladePriority():
    global cladePrio

    keys = [
"primates",
"mammals",
"vertebrate_mammalian",
"birds",
"fish",
"vertebrate",
"vertebrate_other",
"invertebrate",
"plants",
"fungi",
"protozoa",
"viral",
"bacteria",
"archaea",
"other",
"metagenomes",
"n/a",
    ]
    values = [
'01',
'02',
'03',
'04',
'05',
'06',
'07',
'08',
'09',
'10',
'11',
'12',
'13',
'14',
'15',
'16',
'17',
    ]
    cladePrio = dict(zip(keys, values))

####################################################################
### given clade c, return priority from cladePrio
####################################################################
def cladePriority(c):
    global cladePrio
    if c not in cladePrio:
        print(f"ERROR: missing clade {c} in cladePrio")
        sys.exit(255)

    return cladePrio[c]

####################################################################
### read one of the NCBI files from
### /hive/data/outside/ncbi/genomes/reports/assembly_summary_{suffix}
####################################################################
def readAsmSummary(suffix, prioExists, comNames, asmIdClade):

    # Initialize a list to hold the dictionaries
    dataList = []

    filePath = "/hive/data/outside/ncbi/genomes/reports/assembly_summary_" + suffix

    with open(filePath, 'r', encoding='utf-8') as file:
        reader = csv.reader(file, delimiter='\t')
        for row in reader:
            if len(row) < 1:
               continue
            if row[0].startswith('#'):
               continue
            if row[0] in prioExists:
               continue
            if len(row) != 38:
                print(f"ERROR: incorrect number of fields in {file}")
                sys.exit(255)
            gcAccession = row[0]
            strain = re.sub(r'breed=', '', row[8])
            s0 = re.sub(r'cultivar=', '', strain)
            strain = re.sub(r'ecotype=', '', s0)
            s0 = re.sub(r'strain=', '', strain)
            strain = re.sub(r'na', '', s0)
            year = re.sub(r'/.*', '', row[14])
            asmName = row[15]
            asmId = gcAccession + "_" + asmName
            asmSubmitter = row[16]
            asmType = row[23]
            commonName = "n/a"
            if gcAccession in comNames:
                commonName = comNames[gcAccession]
            clade = row[24]	# almost like GenArk clades
            if asmId in asmIdClade:	# specific GenArk clade
                clade = asmIdClade[asmId]
            if clade == "plant":
                clade = "plants"
            cladeP = cladePriority(clade)

            dataDict = {
                "gcAccession": gcAccession,
                "asmName": asmName,
                "scientificName": row[7],
                "commonName": commonName,
                "taxId": row[5],
                "clade": clade,
                "other": asmSubmitter + " " + strain + " " + asmType + " " + year,
                "sortOrder": cladeP,
            }

            utf8Encoded= {k: v.encode('utf-8', 'ignore').decode('utf-8') if isinstance(v, str) else v for k, v in dataDict.items()}
            # Append the dictionary to the list
            dataList.append(utf8Encoded)

    return sorted(dataList, key=lambda x: x['sortOrder'])

####################################################################
### given a URL to hgdownload file: /hubs/UCSC_GI.assemblyHubList.txt
### this sets up the GenArk listing
####################################################################
def readGenArkData(url):
    # Initialize a list to hold the dictionaries
    dataList = []

    response = requests.get(url)
    response.raise_for_status()
    fileContent = response.text
    fileIo = StringIO(fileContent)
    reader = csv.reader(fileIo, delimiter='\t')

    for row in reader:
        if row and row[0].startswith('#'):
            continue
        clade = re.sub(r'\(L\)$', '', row[5])
        cladeP = cladePriority(clade)
        dataDict = {
            "gcAccession": row[0],
            "asmName": row[1],
            "scientificName": row[2],
            "commonName": row[3],
            "taxId": row[4],
            "clade": clade,
            "sortOrder": cladeP,
        }

        utf8Encoded= {k: v.encode('utf-8', 'ignore').decode('utf-8') if isinstance(v, str) else v for k, v in dataDict.items()}
        # Append the dictionary to the list
        dataList.append(utf8Encoded)

    # reset the list so that accessions such as GCF_000001405.40
    # come before GCF_000001405.39
#    dataList.reverse()
#    return dataList
    return sorted(dataList, key=lambda x: x['sortOrder'])

####################################################################
### a manually maintained clade listing for UCSC dbDb assemblies
####################################################################
def dbDbCladeList(filePath):
    returnList = {}

    with open(filePath, 'r') as file:
        reader = csv.reader(file, delimiter='\t')
        for row in reader:
            if len(row) < 1:
               continue
            if row[0].startswith('#'):
               continue
            returnList[row[0]] = row[1]

    return returnList

####################################################################
### out of the genArkData list, extract the given clade
####################################################################
def extractClade(clade, genArkData):
    tmpList = {}
    for item in genArkData:
        if clade != item['clade']:
            continue
        tmpList[item['gcAccession']] = item['commonName']

    # return sorted list on the common name, case insensitive
    returnList = dict(sorted(tmpList.items(), key=lambda item:item[1].lower()))

    return returnList

####################################################################
### Define a key function for sorting by the first word, case insensitive
####################################################################
def getFirstWordCaseInsensitive(row):
    firstWord = row.split('\t')[0]  # Extract the first word
    return firstWord.lower()  # Convert to lowercase for case-insensitive sorting

####################################################################
### process the hgcentral.dbDb data into a dictionary
####################################################################
def processDbDbData(data, clades):
    # Initialize a list to hold the dictionaries
    dataList = []

    # Split the data into lines (rows)
    rows = data.strip().split('\n')
    # reverse the rows so that names such as hg19 come before hg18
    sortedRows = sorted(rows, key=getFirstWordCaseInsensitive, reverse=True)

    for row in sortedRows:
        # Split each row into columns
        columns = row.split('\t')
        clade = clades.get(columns[0], "n/a")
        cladeP = cladePriority(clade)

        # corresponds with the SELECT statement
        # name,scientificName,organism,taxId,sourceName,description
        # Create a dictionary for each row
        dataDict = {
            "name": columns[0],
            "scientificName": columns[1],
            "organism": columns[2],
            "taxId": columns[3],
            "sourceName": columns[4],
            "description": columns[5],
            "clade": clade,
            "sortOrder": cladeP,
        }

        utf8Encoded= {k: v.encode('utf-8', 'ignore').decode('utf-8') if isinstance(v, str) else v for k, v in dataDict.items()}
        # Append the dictionary to the list
        dataList.append(utf8Encoded)

    return sorted(dataList, key=lambda x: x['sortOrder'])

####################################################################
### Function to remove non-alphanumeric characters
### cleans up names to make them better indexing words
####################################################################
def removeNonAlphanumeric(s):
    # Ensure string type
    if isinstance(s, str):
        reSub = re.sub(r'[^a-zA-Z0-9_]', ' ', s).strip()
        return re.sub(r'\s+', ' ', reSub)
    else:
        return s  # Return as-is for non-string types

####################################################################
def eliminateDupWords(s):
    # Split the sentence into words
    words = s.split()

    # Initialize a set to keep track of unique words encountered
    seenWords = set()

    # List to store the words with duplicates removed
    resultWords = []

    for word in words:
        # Convert word to lowercase for case-insensitive comparison
        lowerWord = word.lower()

        # Check if the lowercase version of the word has been seen before
        if lowerWord not in seenWords:
            # If not seen, add it to the result list and mark as seen
            resultWords.append(word)
            seenWords.add(lowerWord)

    # Join the words back into a single string
    return ' '.join(resultWords)

####################################################################
### given a genark accession, return pathname to hub.txt
###  given:   GCA_000001905.1
###  returns: GCA/000/001/905/GCA_000001905.1/hub.txt
####################################################################
def genarkPath(gcAccession):
    # Extract the prefix and the numeric part
    prefix, numPart = gcAccession.split('_')

    # Break the numeric part into chunks of three digits
    parts = [numPart[i:i+3] for i in range(0, len(numPart), 3)]

    # Join the parts to form the path
    path = f"{prefix}/{parts[0]}/{parts[1]}/{parts[2]}/{gcAccession}/hub.txt"

    return path

####################################################################
### for the genArk set, establish some ad-hoc priorities
####################################################################
def establishPriorities(dbDb, genArk):
    global topPriorities
    global allPriorities
    global priorityCounter

    totalItemCount = 0

    expectedTotal = len(dbDb) + len(genArk)
    print(f"### expected total: {expectedTotal:4} = {len(dbDb):4} dbDb genomes + {len(genArk):4} genArk genomes")

    # first priority are the specific manually selected top items
    itemCount = 0
    for name, priority in topPriorities.items():
       allPriorities[name] = priority
       itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\ttopPriorities count: {itemCount:4}")

    primateList = extractClade('primates', genArk)
    mammalList = extractClade('mammals', genArk)

    versionScan = {}	# key is dbDb name without number version extension,
                        # value is highest version number seen for this bare
                        # name
    highestVersion = {}	# key is dbDb name without number version extension,
			# value is the full dbDb name for the highest version
                        # of this dbDb name
    allDbDbNames = {}	# key is the full dbDb name, value is its version

    itemCount = 0
    # scanning the dbDb entries, figure out the highest version number
    #    of each name
    for item in dbDb:
        dbDbName = item['name']
        splitMatch = re.match("([a-zA-Z]+)(\d+)", dbDbName)
        if splitMatch:
            allDbDbNames[dbDbName] = splitMatch.group(2)
            itemCount += 1
            if splitMatch.group(1) in versionScan:
                if float(splitMatch.group(2)) > float(versionScan[splitMatch.group(1)]):
                    versionScan[splitMatch.group(1)] = splitMatch.group(2)
                    highestVersion[splitMatch.group(1)] = dbDbName
            else:
                versionScan[splitMatch.group(1)] = splitMatch.group(2)
                highestVersion[splitMatch.group(1)] = dbDbName
        else:
            print("### dbDb name does not split: ", dbDbName)
            allDbDbNames[dbDbName] = 0
            itemCount += 1

    dbDbLen = len(dbDb)

    # second priority are the highest versioned database primates
    # but not homo sapiens since we already have hg38 in topPriorities
    itemCount = 0
    sortByValue = sorted(versionScan.items(), key=lambda x: x[1], reverse=True)
    for key in sortByValue:
        highVersion = highestVersion[key[0]]
        if highVersion not in allPriorities:
        # find the element in the dbDb list that matches this highVersion name
            highDict = next((d for d in dbDb if d.get('name') == highVersion), None)
            if highDict['clade'] == "primates":
                if highDict['scientificName'].lower() != "homo sapiens":
                    allPriorities[highVersion] = priorityCounter
                    priorityCounter += 1
                    itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tdbDb highest version primates count: {itemCount:4}")

    itemCount = 0
    # and now the GenArk GCF/RefSeq homo sapiens should be lined up here next
    for item in genArk:
        gcAccession = item['gcAccession']
        if not gcAccession.startswith("GCF_"):
            continue
        if gcAccession not in allPriorities:
            sciName = item['scientificName']
            if sciName.lower() == "homo sapiens":
                allPriorities[gcAccession] = priorityCounter
                priorityCounter += 1
                itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tgenArk GCF homo sapiens count: {itemCount:4}")

    itemCount = 0
    # and now the GenArk GCA/GenBank homo sapiens should be lined up here next
    #   GCA/GenBank second
    for item in genArk:
        gcAccession = item['gcAccession']
        if not gcAccession.startswith("GCA_"):
            continue
        if gcAccession not in allPriorities:
            sciName = item['scientificName']
            if sciName.lower() == "homo sapiens":
                allPriorities[gcAccession] = priorityCounter
                priorityCounter += 1
                itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tgenArk GCA homo sapiens count: {itemCount:4}")

    itemCount = 0
    # the primates, GCF/RefSeq first
    for asmId, commonName in primateList.items():
        gcAcc = asmId.split('_')[0] + "_" + asmId.split('_')[1]
        if not gcAcc.startswith("GCF_"):
            continue
        if gcAcc not in allPriorities:
            allPriorities[gcAcc] = priorityCounter
            priorityCounter += 1
            itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tgenArk GCF primates count: {itemCount:4}")

    itemCount = 0
    # and the GCA/GenBank primates
    for asmId, commonName in primateList.items():
        gcAcc = asmId.split('_')[0] + "_" + asmId.split('_')[1]
        if not gcAcc.startswith("GCA_"):
            continue
        if gcAcc not in allPriorities:
            allPriorities[gcAcc] = priorityCounter
            priorityCounter += 1
            itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tgenArk GCA primates count: {itemCount:4}")

    # next are the highest versioned database mammals
    itemCount = 0
    sortByValue = sorted(versionScan.items(), key=lambda x: x[1], reverse=True)
    for key in sortByValue:
        highVersion = highestVersion[key[0]]
        if highVersion not in allPriorities:
        # find the element in the dbDb list that matches this highVersion name
            highDict = next((d for d in dbDb if d.get('name') == highVersion), None)
            if highDict['clade'] == "mammals":
                allPriorities[highVersion] = priorityCounter
                priorityCounter += 1
                itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tdbDb highest version mammals count: {itemCount:4}")

    itemCount = 0
    # the mammals, GCF/RefSeq first
    for asmId, commonName in mammalList.items():
        gcAcc = asmId.split('_')[0] + "_" + asmId.split('_')[1]
        if not gcAcc.startswith("GCF_"):
            continue
        if gcAcc not in allPriorities:
            allPriorities[gcAcc] = priorityCounter
            priorityCounter += 1
            itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tgenArk GCF mammals count: {itemCount:4}")

    itemCount = 0
    # and the GCA/GenBank mammals
    for asmId, commonName in mammalList.items():
        gcAcc = asmId.split('_')[0] + "_" + asmId.split('_')[1]
        if not gcAcc.startswith("GCA_"):
            continue
        if gcAcc not in allPriorities:
            allPriorities[gcAcc] = priorityCounter
            priorityCounter += 1
            itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tgenArk GCA mammals count: {itemCount:4}")

    itemCount = 0
    # dbDb is in cladeOrder, process in that order, find highest versions
    for item in dbDb:
        dbDbName = item['name']
        if dbDbName not in allPriorities:
            splitMatch = re.match("([a-zA-Z]+)(\d+)", dbDbName)
            if splitMatch:
                noVersion = splitMatch.group(1)
                version = allDbDbNames[dbDbName]
                highVersion = versionScan[noVersion]
                if highVersion == version:
                    allPriorities[dbDbName] = priorityCounter
                    priorityCounter += 1
                    itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tdbDb highest versions count: {itemCount:4}")

    itemCount = 0
    # GCF RefSeq from GenArk next priority
    for item in genArk:
        gcAccession = item['gcAccession']
        if not gcAccession.startswith("GCF_"):
            continue
        if gcAccession not in allPriorities:
            allPriorities[gcAccession] = priorityCounter
            priorityCounter += 1
            itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tgenArk GCF count: {itemCount:4}")

    itemCount = 0
    # GCA GenBank from GenArk next priority
    for item in genArk:
        gcAccession = item['gcAccession']
        if not gcAccession.startswith("GCA_"):
            continue
        if gcAccession not in allPriorities:
            allPriorities[gcAccession] = priorityCounter
            priorityCounter += 1
            itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tgenArk GCA count: {itemCount:4}")

    itemCount = 0
    for entry in dbDb:
        dbName = entry['name']
        if dbName not in allPriorities:
            allPriorities[dbName] = priorityCounter
            priorityCounter += 1
            itemCount += 1
    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tthe rest of dbDb count: {itemCount:4}")

####################################################################

"""
table load procedure:

  hgsql -e 'DROP TABLE IF EXISTS genomePriority;' hgcentraltest
  hgsql hgcentraltest < ../lib/genomePriority.sql
  hgsql -e 'LOAD DATA LOCAL INFILE "genomePriority.tsv" INTO
TABLE genomePriority;' hgcentraltest
  hgsql -e 'ALTER TABLE genomePriority
ADD FULLTEXT INDEX gdIx
(name, commonName, scientificName, clade, description);' \
  hgcentraltest

"""

####################################################################
def main():
    global priorityCounter
    global allPriorities

    if len(sys.argv) != 2:
        print("genomePriority.py - prepare genomePriority.tsv file from")
        print("    dbDb.hgcentral and UCSC_GI.assemblyHubList.txt file.\n")
        print("Usage: genomePriority.py dbDb.name.clade.tsv\n")
        print("the dbDb.name.clade.tsv file is a manually curated file to relate")
        print("    UCSC database names to GenArk clades, in source tree hubApi/")
        print("This script is going to read the dbDb.hgcentral table, and the file")
        print("    UCSC_GI.assemblyHubList.txt from hgdownload.")
        print("Writing an output file genomePriority.tsv to be loaded into")
        print("    genomePriority.hgcentral.  See notes in this script for load procedure.")
        sys.exit(255)

    # Ensure stdout and stderr use UTF-8 encoding
    set_utf8_encoding()

    initCladePriority()

    dbDbNameCladeFile = sys.argv[1]

    # the correspondence of dbDb names to GenArk clade categories
    dbDbClades = dbDbCladeList(dbDbNameCladeFile)
    # Get the dbDb.hgcentral table data
    rawData = dbDbData()
    dbDbItems = processDbDbData(rawData, dbDbClades)

    # read the GenArk data from hgdownload into a list of dictionaries
    genArkUrl = "https://hgdownload.soe.ucsc.edu/hubs/UCSC_GI.assemblyHubList.txt"
    genArkItems = readGenArkData(genArkUrl)

    establishPriorities(dbDbItems, genArkItems)

    asmIdClade = readAsmIdClade()
    commonNames = allCommonNames()
    print("# all common names: ", len(commonNames))

    refSeqList = readAsmSummary("refseq.txt", allPriorities, commonNames, asmIdClade)
    print("# refSeq assemblies: ", len(refSeqList))
    genBankList = readAsmSummary("genbank.txt", allPriorities, commonNames, asmIdClade)
    print("# genBank assemblies: ", len(genBankList))

    refSeqGenBankList = refSeqList + genBankList
    print("# refSeq + genBank assemblies: ", len(refSeqGenBankList))

    refSeqGenBankSorted =  sorted(refSeqGenBankList, key=lambda x: x['sortOrder'])
    print("# sorted refSeq + genBank assemblies: ", len(refSeqGenBankSorted))


    outFile = "genomePriority.tsv"
    fileOut = open(outFile, 'w')

    totalItemCount = 0
    itemCount = 0
    # Print the dbDb data
    for entry in dbDbItems:
        dbDbName = entry['name']
        if dbDbName in allPriorities:
            priority = allPriorities[dbDbName]
        else:
            print("no priority for ", dbDbName)
            sys.exit(255)

        clade = entry['clade']

        descr = f"{entry['sourceName']} {entry['description']}\n"
        description = re.sub(r'\s+', ' ', descr).strip()
        outLine =f"{entry['name']}\t{priority}\t{entry['organism']}\t{entry['scientificName']}\t{entry['taxId']}\t{clade}\t{description}\t1\t\n"
        fileOut.write(outLine)
        itemCount += 1

    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tdbDb count: {itemCount:4}")

    itemCount = 0
    # Print the GenArk data
    for entry in genArkItems:
        gcAccession = entry['gcAccession']
        if gcAccession in allPriorities:
            priority = allPriorities[gcAccession]
        else:
            print("no priority for ", gcAccession)
            sys.exit(255)

        hubPath = genarkPath(gcAccession)
        cleanName = removeNonAlphanumeric(entry['commonName'])
        clade = entry['clade']
        descr = f"{entry['asmName']}"
        description = re.sub(r'\s+', ' ', descr).strip()
        outLine = f"{entry['gcAccession']}\t{priority}\t{entry['commonName'].encode('ascii', 'ignore').decode('ascii')}\t{entry['scientificName']}\t{entry['taxId']}\t{clade}\t{description}\t1\t{hubPath}\n"
        fileOut.write(outLine)
        itemCount += 1

    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\tgenArk count: {itemCount:4}")

    incrementPriority = len(allPriorities) + 1
    print("# incrementing priorities from: ", incrementPriority)

    itemCount = 0
    # Print the refSeq/genBank data
    for entry in refSeqGenBankSorted:
        gcAccession = entry['gcAccession']
        commonName = entry['commonName']
        scientificName = entry['scientificName']
        asmName = entry['asmName']
        clade = entry['clade']
        descr = f"{asmName} {entry['other']}"
        description = re.sub(r'\s+', ' ', descr).strip()
        outLine = f"{entry['gcAccession']}\t{incrementPriority}\t{entry['commonName'].encode('ascii', 'ignore').decode('ascii')}\t{entry['scientificName']}\t{entry['taxId']}\t{clade}\t{description.encode('ascii', 'ignore').decode('ascii')}\t0\t\n"
        fileOut.write(outLine)
        incrementPriority += 1
        itemCount += 1

    totalItemCount += itemCount
    print(f"{totalItemCount:4} - total\trefSeq + genbank count: {itemCount:4}")

    fileOut.close()

if __name__ == "__main__":
    main()
