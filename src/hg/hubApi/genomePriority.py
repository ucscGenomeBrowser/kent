#!/cluster/software/bin/python3

import subprocess
import locale
import sys
import re
import os
import csv
import requests
from io import StringIO

# special top priorities 
topPriorities = {
    'hg38': 1,
    'mm39': 2,
    'hs1': 3,
}

### key will be dbDb name, value will be priority number
allPriorities = {}

priorityCounter = len(topPriorities) + 1

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
### given a URL to hgdownload file: /hubs/UCSC_GI.assemblyHubList.txt
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
        dataDict = {
            "gcAccession": row[0],
            "asmName": row[1],
            "scientificName": row[2],
            "commonName": row[3],
            "taxId": row[4],
            "clade": re.sub(r'\(L\)$', '', row[5]),
        }
        
        utf8Encoded= {k: v.encode('utf-8', 'ignore').decode('utf-8') if isinstance(v, str) else v for k, v in dataDict.items()}
        # Append the dictionary to the list
        dataList.append(utf8Encoded)

    # reset the list so that accessions such as GCF_000001405.40
    # come before GCF_000001405.39
    dataList.reverse()
    return dataList

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
# Define a key function for sorting by the first word, case insensitive
def getFirstWordCaseInsensitive(row):
    firstWord = row.split('\t')[0]  # Extract the first word
    return firstWord.lower()  # Convert to lowercase for case-insensitive sorting

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
        }
        
        utf8Encoded= {k: v.encode('utf-8', 'ignore').decode('utf-8') if isinstance(v, str) else v for k, v in dataDict.items()}
        # Append the dictionary to the list
        dataList.append(utf8Encoded)
    
    return dataList

####################################################################
# Function to remove non-alphanumeric characters
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
                if splitMatch.group(2) > versionScan[splitMatch.group(1)]:
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
    # the rest of the highest versions of each unique dbDb name
    sortByValue = sorted(versionScan.items(), key=lambda x: x[1], reverse=True)
    for key in sortByValue:
        highVersion = highestVersion[key[0]]
        if highVersion not in allPriorities:
            allPriorities[highVersion] = priorityCounter
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
    # finally the rest of the database genomes names
    for dbName in sorted(allDbDbNames):
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
  hgsql hgcentraltest < genomePriority.sql
  hgsql -e 'LOAD DATA LOCAL INFILE "genomePriority.tsv" INTO
TABLE genomePriority;' hgcentraltest
  hgsql -e 'ALTER TABLE genomePriority
ADD FULLTEXT INDEX gdIx
(name, commonName, scientificName, description);' hgcentraltest
"""
####################################################################

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
        sys.exit(-1)

    dbDbNameCladeFile = sys.argv[1]

    # Ensure stdout and stderr use UTF-8 encoding
    set_utf8_encoding()

    # the correspondence of dbDb names to GenArk clade categories
    dbDbClades = dbDbCladeList(dbDbNameCladeFile)
    # Get the dbDb.hgcentral table data
    rawData = dbDbData()
    dbDbItems = processDbDbData(rawData, dbDbClades)

    # read the GenArk data from hgdownload into a list of dictionaries
    genArkUrl = "https://hgdownload.soe.ucsc.edu/hubs/UCSC_GI.assemblyHubList.txt"
    genArkItems = readGenArkData(genArkUrl)

    establishPriorities(dbDbItems, genArkItems)

    outFile = "genomePriority.tsv"
    fileOut = open(outFile, 'w')

    itemCount = 0
    # Print the dbDb data
    for entry in dbDbItems:
        dbDbName = entry['name']
        if dbDbName in allPriorities:
            priority = allPriorities[dbDbName]
        else:
            print("no priority for ", dbDbName)

        clade = entry['clade']

        descr = f"{entry['sourceName']} {clade} {entry['taxId']} {entry['description']}\n"
        description = re.sub(r'\s+', ' ', descr).strip()
        outLine =f"{entry['name']}\t{priority}\t{entry['organism']}\t{entry['scientificName']}\t{entry['taxId']}\t{description}\n"
        fileOut.write(outLine)
        itemCount += 1

    itemCount = 0
    # Print the GenArk data
    for entry in genArkItems:
        gcAccession = entry['gcAccession']
        if gcAccession in allPriorities:
            priority = allPriorities[gcAccession]
        else:
            print("no priority for ", gcAccession)

        cleanName = removeNonAlphanumeric(entry['commonName'])
        clade = entry['clade']
        descr = f"{entry['asmName']} {clade} {entry['taxId']}\n"
        description = re.sub(r'\s+', ' ', descr).strip()
        outLine = f"{entry['gcAccession']}\t{priority}\t{entry['commonName'].encode('ascii', 'ignore').decode('ascii')}\t{entry['scientificName']}\t{entry['taxId']}\t{description}\n"
        fileOut.write(outLine)
        itemCount += 1

    fileOut.close()

if __name__ == "__main__":
    main()
