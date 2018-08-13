#! /usr/bin/env python3.6

import re, argparse, os, gzip, sys
from collections import Counter, defaultdict
from urllib import parse

#####
# Define dictionaries for recording usage information
#####

# General search term dictionaries
searchTermUsers = defaultdict(Counter)
#searchTermCounts = defaultdict()

# HGVS search term dictionaries
hgvsTermUsers = defaultdict(Counter)
#hgvsTermCounts = defaultdict()

#####
#####


#####
# Define set of Brian Lee's HGVS test search terms
#####
hgvsTestTerms = {"NM_000310.3(PPT1):c.271_287del17insTT", "NM_007262.4(PARK7):c.-24+75_-24+92dup",
                "NM_006172.3(NPPA):c.456_*1delAA", "MYH11:c.503-14_503-12del", "NM_198576.3(AGRN):c.1057C>T",
                "NM_198056.2:c.1654G>T", "NP_002993.1:p.Asp92Glu", "NP_002993.1:p.D92E",
                "BRCA1 Ala744Cys", "NM_000828.4:c.-2G>A"}


def findSearchTerms(line):
    """Find and record usage information about search terms found in log lines"""
    splitLine = line.rstrip().split(" ")
    ip = splitLine[0]
    line = parse.unquote(line)
    output = re.findall("hgt\.positionInput\=[^&]+&?", line)
    # If we find search terms in line, process them
    if output != []:
        # Stringify search terms and remove extra characters
        output = str(output).replace("&", "").replace("[", "").replace("]", "").replace("'", "")
        # Split on "=" sign and keep only part after this sign
        outputList = output.split("=")
        searchTerm = outputList[1]
        # Count up search term use per user/IP address
        searchTermUsers[searchTerm][ip] += 1

        # Find search terms like NM_000310.3(PPT1):c.271_287del17insTT
        hgvs = re.findall("^[N][M|R|P]_[0-9]*\.[\(\)A-Z0-9]*:[a-z]\..*", searchTerm)
        if hgvs == []:
        # Find search terms like MYH11:c.503-14_503-12del
            hgvs = re.findall("^[A-Z0-9]*\:[a-z]\..*", searchTerm)
        if hgvs == []:
        # Find search terms like RCA1 Ala744Cys
            hgvs = re.findall("^[A-Z0-9]*\s[A-Z][a-z][a-z][0-9].*[A-Z][a-z][a-z]", searchTerm)
        # If we have HGVS results, stringify them and remove extra characters
        if hgvs != []:
            hgvs = str(hgvs).replace("&", "").replace("[", "").replace("]", "").replace("'", "")
            if hgvs in hgvsTestTerms:
                hgvs = "** " + hgvs 
            hgvsTermUsers[hgvs][ip] += 1

def processFile(fileName, gzipped=False):
    """Process hgTracks lines in a file"""
    # Need to process gzipped files differently
    if gzipped == True:
        for line in gzip.open(fileName):
            line = line.decode("ASCII")
            # Only process lines with search terms in them (lines w/ "positionInput"
            if "positionInput" in line:
                findSearchTerms(line)
        # Output results to file
        outputToFile(searchTermUsers, "searchTermsUsers.tsv", "searchTermCounts.tsv")
        outputToFile(hgvsTermUsers, "hgvsTermsUsers.tsv", "hgvsTermCounts.tsv")

    # Process all other files
    else: 
        for line in open(fileName):
            if "positionInput" in line:
                findSearchTerms(line)
        # Output results to file
        outputToFile(searchTermUsers, "searchTermsUsers.tsv", "searchTermCounts.tsv")
        outputToFile(hgvsTermUsers, "hgvsTermsUsers.tsv", "hgvsTermCounts.tsv")

def processDir(dirName, gzipped=False):
    """Process files in a directory one-by-one using processFile function"""
    fileNames = os.listdir(dirName)
    for log in fileNames:
        processFile(dirName + "/" + log, gzipped)


def outputToFile(usersDict, usersOutName, countsOutName):
    """Output supplied dictionary to a tab-separated file"""
    usersOut = open(usersOutName, "w")
    countsOut = open(countsOutName, "w")

    usersOut.write("# User/IP\tSearch term\tUse count\n")
    countsOut.write("# Search term\t# of users\t# of times searched\n")

    for term in usersDict:
        users = 0
        totalUse = 0
        for user in usersDict[term]:
            count = usersDict[term][user]
            if count!= 1:
                users += 1
                totalUse += count
                usersOut.write(str(user) + "\t" + term + "\t" + str(count) + "\n")

        if users != 0 or totalUse != 0:
            countsOut.write(term + "\t" +  str(users) + "\t" + str(totalUse) + "\n")

def dumpToJson(data, outJsonName):
    """Dump supplied dictionary to a JSON format file"""
    jsonOut = open(outJsonName, "w")
    json.dump(data, jsonOut)
    jsonOut.close()


def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("-f","--fileName", type=str, help='input file name, must be space-separated Apache access_log file')
    parser.add_argument("-d","--dirName", type=str , help='input directory name, files must be space-separated Apache access_log files. No other files should be present in this directory.')
    parser.add_argument("-g","--gzipped", action='store_true', help='indicates that input files are gzipped (.gz)')
    args = parser.parse_args()


    if args.fileName != None and args.dirName != None:
        print("-f/--fileName and -d/--dirName cannot be used together. Choose one and re-run.")
        sys.exit(1)

    # Process input to create dictionaries containing info about users per db/track/hub track
    if args.fileName:
        processFile(args.fileName, args.gzipped)
    elif args.dirName:
        processDir(args.dirName, args.gzipped)

if __name__ == "__main__":
    main()
