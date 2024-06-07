#!/usr/bin/python3
import argparse
import sys
import subprocess
from io import BytesIO
from zipfile import ZipFile
from urllib.request import urlopen
from ucscGb.qa.tables import trackUtils
from ucscGb.qa import qaUtils

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return (bashStdoutt)

def download(URL):
    '''
    Generic command to wget a URL and prints the command to STDOUT.
    '''
    cmd = "wget %s" % URL
    print("    " + cmd)
    bash(cmd)

def analyzePFM(PFMzip):
    '''
    Given a URL to the PFM matrix for the JASPAR hub, read each line and calculate the
    nucleotide percents at each position for the Motif display. The function returns a dicitonary
    with the motif name as the key and a tuple as the value (the percents and column counts 
    '''
    archive = ZipFile(BytesIO(urlopen(PFMzip).read()),'r')
    PFMtable = {}
    for eachFile in archive.namelist(): # Iterate through each of files
        aList = [] 
        cList = []
        gList = []
        tList = []
        numOfColumns = 0 # container for the number of columns
        for line in archive.open(eachFile):
            currentLine = line.decode('utf-8').strip() #convert to utf-8
            if currentLine.startswith('>'):
                continue  # Skip the header line
            # Matrix line creates a list of seen values
            matrix = currentLine.split("[")[1].strip().split(']')[0].strip().split()
            numOfColumns = len(matrix)
            if currentLine.startswith('A'):
                aList = [int(x) for x in matrix]
            elif currentLine.startswith('C'):
                cList = [int(x) for x in matrix]
            elif currentLine.startswith('G'):
                gList = [int(x) for x in matrix]
            elif currentLine.startswith('T'):
                tList = [int(x) for x in matrix]
            else:
                print ("Line not recognized")
        aPercent = []
        cPercent = []
        gPercent = []
        tPercent = []
        for A,C,G,T in zip(aList,cList,gList,tList): # Go through each of the counts in the matrix
            total = A+C+T+G # Calcualte the column total
            aPercent.append(float("%.6g" % (A/total)))
            cPercent.append(float("%.6g" % (C/total)))
            gPercent.append(float("%.6g" % (G/total)))
            tPercent.append(float("%.6g" % (T/total)))

        #remove .jaspar from the filename
        matrixName = eachFile[:-7]
        percents = str(aPercent).strip("[]") + "\t"
        percents += str(cPercent).strip("[]") + "\t"
        percents += str(gPercent).strip("[]") + "\t"
        percents += str(tPercent).strip("[]") # Add each percent as a tab-separated column
         
        PFMtable[matrixName] = percents, numOfColumns # Add the matrix to the dictionary
    return PFMtable

def getTrackDb(db):
    '''
    Read through the trackDb file. This generator reads through the first stanza in the trackDb
    file, and returns the key/value pairs. Upon finding the first empty new line, the generator
    stops. (Could lead to a possible bug if the first line is empty.)
    '''
    trackDb = "https://frigg.uio.no/JASPAR/JASPAR_genome_browser_tracks/current/%s/trackDb.txt" % db
    try:
        for line in urlopen(trackDb):
            key,value = line.decode('utf-8').strip().split(" ", 1) # split using first space
            yield key,value
    except ValueError:
        return key,value

def buildTrackDb(db):
    '''
    Given a database, creates a trackDb for the JASPAR track for the UCSC database. Modifies
    settings that were specific to the hub and changes them to fit our architechure.
    '''
    newTrackDb = [] #final container to return
    year = "" #container for this years release
    for setting, value in getTrackDb(db): # Make a unified trackDb using hg38
        if setting == "track":
            newTrackName = value.split("_")[0].lower() #edit the track name to how we have it
            year = newTrackName[6:]
            newTrackDb.append([setting,newTrackName])
            newTrackDb.append(["parent", "jaspar on"]) #Adding settings that aren't in the hub
            newTrackDb.append(["priority","1"])
        elif setting == "shortLabel":
            shortLabel = "JASPAR %s TFBS" % year
            newTrackDb.append([setting, shortLabel])
        elif setting == "longLabel":
            longLabel = "JASPAR CORE %s - Predicted Transcription Factor Binding Sites" % year
            newTrackDb.append([setting, longLabel])
        elif setting == "html": #skip the HTML section for now
            continue
        elif setting == "bigDataUrl": # Make the bigDataUrl point to /gbdb
            #download file
            path = "/gbdb/$D/jaspar/JASPAR%s.bb" % year
            newTrackDb.append([setting, path])
        elif setting == "filterValues.name":
            # test to make sure the filters are in alphebetical order
            sortedFilter = sorted(value.split(","), key=str.casefold)
            newTrackDb.append([setting, ",".join(sortedFilter)])
        else:
            newTrackDb.append([setting,value])

    return newTrackDb

def main(args):
    '''
    Program to automate the update of the JASPAR tracks for mm10, mm39, hg19, and hg38.

    Assumptions:
       * Shared HTML, PFMs, and bigBeds are all obtained from:
         https://frigg.uio.no/JASPAR/JASPAR_genome_browser_tracks/current/
    '''
    # Setting some definitions
    assemblies= ["hg19", "hg38", "mm10", "mm39"]
    # Any broken links in the script can be fixed below.
    currentAddress = "https://frigg.uio.no/JASPAR/JASPAR_genome_browser_tracks/current/"
    PFMs = currentAddress + "PFMs.zip"
    sharedHtml = currentAddress + "JASPAR2024_TFBS_help.html"
    bigBedUrl = currentAddress + "%s/JASPAR2024_%s.bb"

    #################################################################
    #                           Build trackDb                       #
    #################################################################
    print ("########################################################")
    print ("#            Creating the new trackDb stanza           #")
    print ("########################################################")
    newTrackDb = buildTrackDb("hg38") #container for all trackDb settings
           
    #################################################################
    #                 Print trackDb to the file                     #
    #################################################################
    trackDb = open("jaspar.ra", "w")
    for setting,value in newTrackDb:
        print("\t",setting,value, file=trackDb)
    trackDb.close()
    print("    Done!")
    
    #################################################################
    #          Read the PFMs via URL and calculate matrix           #
    #################################################################
    print ("########################################################")
    print ("#       Reading PFM.zip file and anlayzing matrix      #")
    print ("########################################################")
    hgFixed = open("jasparMotif.tab", 'w')
    completeMatrix = analyzePFM(PFMs)
    for matrixName, (percents, numOfColumns) in completeMatrix.items():
        print (matrixName, numOfColumns, percents, sep="\t", file=hgFixed)

    print("    Done!")

    #################################################################
    #           Download any files needed for the update            #
    #################################################################
    print ("########################################################")
    print ("#             Downloading HTML files                   #")
    print ("########################################################")
    print ("Downloading HTML page:")
    download(sharedHtml) # Download the HTML file
    print ("Done!")
    print ("########################################################")
    print ("#             Downloading bigBed files                 #")
    print ("########################################################")
    for db in assemblies:
        print ("Downloading the bigBed for %s" % db)
        bigBed = bigBedUrl % (db,db) # Use the correct URL for the current database
        download(bigBed) # Download the file
        print ("Done!", end="\n\n")

    #################################################################
    #         Create a file with notes about how to update          #
    #################################################################
    print ("########################################################")
    print ("#            Next steps to complete update             #")
    print ("########################################################")
    print (" updateSteps.txt - contains instructions on how to")
    print ("                   finish the update.")
    steps = open("updateSteps.txt",'w') # file to write all steps
    print ("########################################################", file=steps)
    print ("#            Next steps to complete update             #", file=steps)
    print ("########################################################", file=steps)
    print ("1. Add trackDb statment to jaspar.ra", file=steps)
    print ("2. Review HTML page and add new changes to the existing HTML page", file=steps)
    print ("3. Move bigBed files to /hive/data/genomes/$DB/bed/jaspar", file=steps)
    print ("4. Create a symlink for the bigBed in /gbdb/$DB/jaspar/", file=steps)
    print ("5. Create the hgFixed table for the PFM image on hgc, i.e.", file=steps)
    print ("   hgLoadSqlTab hgFixed jasparCore2024 ~/kent/src/hg/lib/dnaMotif.sql jasparMotif.tab", file=steps)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
