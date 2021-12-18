#!/usr/bin/python3

'''
*******************************************************************************
* Script to help determine which links need updating from the uiLinks cronjob
* through an interactive shell. This program requires the assembly being checked
* by the cronjob and a file that contains the raw uiLinks cronjob output without
* the summary section, i.e.,
*
*     Summary
*     ======= =======
*     85 pages checked
*     8 pages with errors found
*
* The output of this script is text to STDOUT and the creation of a file:
*
*     python-database.dict  -- Storing checked links for future checks
*
*******************************************************************************
'''
import argparse
import subprocess
import collections
from collections import defaultdict
import sys
import os
import time


def get_options():
    '''
    Parses the command line options
    '''
    parser = argparse.ArgumentParser(description=print(__doc__),
                                     usage="%(prog)s " +\
                                           "[--dbOnly, --quick, --resume database] "+\
                                           "cronjob_output")
    parser.add_argument('--dbOnly', action='store_true',
                        help='Only show list of assemblies that need updating.')
    parser.add_argument('--quick', action='store_true',
                        help='Do not do new link checks. Replace with what has been previous seen.')
    parser.add_argument('--resume', metavar="database",
                        help='Skip to the database that needs updating.')
    parser.add_argument('cronjob_output', action='store',
                        help='File containing all affected links.')
    if len(sys.argv) == 1:
        parser.print_help(sys.stderr)
        sys.exit(1)
    return parser.parse_args()

def clean_url(URL):
    '''
    Clean the URL path to remove the ../ and give the correct path
    '''
    urlPath = URL.split("/..") # Check if there are any references to the directory above
    if len(urlPath) == 1: # No ' /.. '  was found
        return urlPath[0] # return the path as normal
    removeNumber = len(urlPath) -1 # Find the number of ' ../ ' in path
    cleanPath = urlPath[0]
    while removeNumber != 0:
        cleanPath = os.path.dirname(cleanPath)
        removeNumber -= 1
    realPath = cleanPath + urlPath[-1]
    return realPath


def replace_links(trackNames, db, myLinks):
    '''
    Get the location of the affected html files in trackDb using the findLevel utility.

    input: list of track names, assembly, dictionary of links
    output: new file with paths to the affected html files 
    '''

    tracksList = [] # Container to prevent editing the same page twice

    for track in trackNames:
        findLevel = subprocess.check_output("findLevel %s %s" % (db,track),
                                            stderr=subprocess.STDOUT,
                                            shell=True)
        myList = findLevel.decode("utf-8").split() # findLevel output
        # Read each line from the findLevel output
        htmlPaths = []
        for entry in myList:
            if entry.endswith(".html") and entry not in tracksList:
                thisPath = clean_url(entry)
                htmlPaths.append(thisPath)

                tracksList.append(thisPath) # Add the path to the list to prevent duplicates
                sharedPath = check_for_shared_html(thisPath)

                # Add any html includes to the list of file to check
                tracksList += sharedPath
                htmlPaths += sharedPath
                '''
                item is the path to the html file
                > Use loop through myLinks and use replace
                '''
                for htmlPath in htmlPaths:
                    for oldLink,newLink in sorted(myLinks.items()):
                        if newLink != "":
                            print("Now editing: %s" % htmlPath, end="\n\n")
                            subprocess.run("replace \"%s\" \"%s\" -- %s" % \
                                           (oldLink, newLink, htmlPath),
                                           shell=True)
		            #replace $oldLink $newLink -- $thisFile % (oldLink, newLink, item)


def check_for_shared_html(htmlPath):
    '''
    Function to check if the file has an insert statement for the shared html file.
    '''
    insertStatement = "#insert file="
    # Check if there is an insert statement
    try:
        grepOutput = subprocess.check_output("grep \"%s\" %s" % (insertStatement,htmlPath),
                                             stderr=subprocess.STDOUT,
                                             shell=True)
    except subprocess.CalledProcessError:
        return [] #Grep did not find an insert statement, return an empty list
    grepList = grepOutput.decode("utf-8").split('\"')
    # Create a list of html files found
    htmlList = [path for path in grepList if ".html" in path]
    cleanPaths = [] # container to return
    htmlCopy = htmlPath # save the path of the HTML file containing the insert statement
    for path in htmlList:
        realPath = ""
        currentPath = path.split("../")
        number = len(currentPath) #Checking if the html path is in a higher directory
        while number > 0: 
            realPath = os.path.dirname(htmlPath)
            htmlPath = realPath
            number -= 1
        cleanPaths.append(realPath + "/" + currentPath[-1]) #Add the HTML path to the container
        htmlPath = htmlCopy # Restore the root path to what was supplied
    return cleanPaths
    

def print_info(assembly, number):
    '''
    Simply print how many links need updating.
       input: assembly and dictionary of links
       output: prints a string
    '''
    print("Now checking links for the %s assembly." % assembly)
    print("The following %d link(s) may need updating." % number)
    print("************* If the link does not need updating, hit ENTER ****************")

def read_dictionary():
    '''
    Read in the dicitonary
    '''
    dictionaryLine = ""
    try:
        previousLinks = open("python-database.dict", 'r')
    except FileNotFoundError:
        return collections.defaultdict(str)
        
    for line in previousLinks.read():
        dictionaryLine += line
    dictionaryLine = dictionaryLine.replace("<class 'str'>", 'str')

    # Previously ran dictionary
    return eval(dictionaryLine)

def read_cronjob(cronjob_file):
    '''
    Function to read the output from the uiLinks cronjob. 

    Input: Raw data file from the cronjob and a dictionary for the reported URLs
    Output: Returns a dictionary of the broken links and a list of possibly affect track names.


    Dev Notes:
        * OrderedDict() may be able to handle reading the different assemblies
        ** popItem(last=True) will return and remove the pair from the dictionary 
    '''
    myFile = open(cronjob_file,'r')
    assembly = ""     # Current assembly being checked
    isNewAssembly = True  # Container for the next assembly
    previousLine= ""  # Container to grab track names (previous line read)
    myLinks = collections.defaultdict(str)
    allTracks = []
    for line in myFile:
        thisLine = line.strip() # Remove any whitespace

        # Finished processing the previous assembly. Current line is a new assembly
        if isNewAssembly == True:
            assembly = thisLine
            # Clear previous info
            isNewAssembly = False
            myLinks = collections.defaultdict(str)
            allTracks = []
            continue

        if thisLine == "==============":
            allTracks.append(previousLine) # Save the track name

        # If the current line starts with http, add it to a dictionary
        # (Will capture https as well)
        if thisLine.startswith('http'):
            if thisLine in myLinks:
                continue # If the link was seen before, skip it.
            ######## Check if the link is in dictionary w/o backslash. Known bug #########
            else:
                myLinks[thisLine] = ""     # Otherwise add it to the dictionary. 
        previousLine = thisLine
        
        if thisLine == "%%%%%%%":
            isNewAssembly = True
            yield assembly, myLinks, allTracks

    # Last assembly was read
    yield assembly, myLinks, allTracks

def do_work(myLinks, previousLinks):
    '''
    This function checks each link in the myLinks dictionary and tries to see if there was a
    previous link that replaced it. Since dictionaries are mutable, nothing is returned.

    Input: Two dictionaries
    Output: Nothing but updated dictionaries in the main loop.

    > Want to add flags for debuging and quick QA (Assumes all previous links are correct.)
    '''

    for link in myLinks.keys():
        print("\nChecking...\n")
        print(link + "\n")

        # Check if the link was previously checked
        if link in previousLinks:
            response = input("Does this link seem reasonable:\n%s\n\nEnter \"YES or Y\"\n> "
                             % previousLinks[link])
            if response.upper() == "YES" or response.upper() == "Y":
                myLinks[link] = previousLinks[link]
                continue

        newLink = input("Please enter the new link:\n> ")
        if newLink == "":
            continue
        while len(newLink) < 5:
            newLink = input("Please enter a valid link:\n> ")
        myLinks[link] = newLink
        previousLinks[link] = newLink # Add the new link to the database

def save_links(previousLinks):
    '''
    This function saves the links check to a file to be used for next round of checks.

    Input: One dictionaries
    Output: Writes to a file

    '''
    print("\n\nThe new links have been saved to a file called: python-database.dict")
    database = open("python-database.dict", "w+")
    database.write(str(previousLinks))
    database.close()


def main():
    '''The main program that is ran'''
    options = get_options()
    date = options.cronjob_output   # Raw uiLinks output
    dbOnly = options.dbOnly         # True/False to display only show databases that need updates
    skipToAssembly = options.resume # Resume from a specified assembly 
    quickQA = options.quick

    #Dictionary to hold links
    '''
    Dev Notes:
        * OrderedDict() may be able to handle reading the different assemblies
        ** popItem(last=True) will return and remove the pair from the dictionary 
    '''
    previousLinks = read_dictionary() # Dictionary of all links every seen
    # Update the myLinks dictionary with the affected links and get a list of track names

    assembliesToUpdate = []

    for assembly, myLinks, allTracks in read_cronjob(date):
        '''
        Read the uiLinks cronjob one assembly at a time. 
        '''
        if skipToAssembly:
            if assembly != skipToAssembly:
                continue
            skipToAssembly = "" # Clear the assembly once you get to it

        totalNumberOfLinks = len(myLinks)
        if totalNumberOfLinks == 0:
            continue # Nothing to do for the current assembly so continue (script does not consider ftp links)

        assembliesToUpdate.append(assembly) # Add the assembly to the list

        # Continue to the next assembly if only listing the assemblies that need updating
        if dbOnly:
            continue

        # Print the number of affected links to the screen
        print_info(assembly, totalNumberOfLinks) 

        # All links should have been read for the assembly
        if quickQA:
            myLinks = previousLinks
        else:
            # Time to start looking for the new links
            do_work(myLinks, previousLinks)

        # Now update each link in myLinks, largest links first.
        # Create a function that uses findLevel and replace so no files are created

        print("Checking if the %s database needs updating..." % assembly, end="\n\n")
        replace_links(allTracks, assembly, myLinks)

    if dbOnly:
        pass
    elif quickQA:
        pass
    else:
        # Save the previousLinks dictionary for future use. 
        save_links(previousLinks)

    print("\n******")
    print("Now update trackDb and friends for the following databases:", end="\n\n")
    print('make alpha DBS="', " ".join(assembliesToUpdate), '"', sep="")
    print("\n******")
    print("Push request list:", end="\n\n")
    print("\n".join(assembliesToUpdate))



if __name__ == "__main__":
    sys.exit(main())
