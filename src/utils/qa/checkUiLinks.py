#!/usr/bin/python3
import subprocess
import collections
from collections import defaultdict
import sys
import os
import argparse


def parseArgs():
    '''
    Parse command-line arguments.
    '''
    parser = argparse.ArgumentParser(
        description="Run the checkTrackUiLinks.csh cronjob for all active assemblies.")
    parser.add_argument("-e", "--errors-only", dest="errorsOnly", action="store_true",
                        help="Only print output for assemblies where link errors were found. "
                             "Assemblies that check out clean are skipped entirely, so no output "
                             "means nothing to report.")
    return parser.parse_args()


def getActiveAssemblies():
    '''
    Return a list of active assemblies on the RR (active=1)
    '''
    host = "genome-centdb"
    command = "select name from dbDb where active=1"
    database = "hgcentral"
    hgsqlOutput = subprocess.run("/cluster/bin/x86_64/hgsql -h %s -Ne '%s' %s" %\
                                         (host, command, database),\
                                         stdout=subprocess.PIPE,\
                                         stderr=subprocess.PIPE,\
                                         shell=True)
    # Convert the binary string to a regular string
    # Then create a list
    myOutput = hgsqlOutput.stdout.decode('utf-8').split()
    # Skip curated hubs
    skipList = ['hs1', 'mpxvRivers']
    for db in skipList:
        if db in myOutput:
            myOutput.remove(db)
    return myOutput

def runCron(assembly):
    '''
    Run the checkTrackUiLinks.csh script and return its output
    '''
    cronOutput = subprocess.run("/cluster/bin/scripts/checkTrackUiLinks.csh %s all" % assembly,\
                                         stdout=subprocess.PIPE,\
                                         stderr=subprocess.PIPE,\
                                         shell=True)
    myOutput = cronOutput.stdout.decode('utf-8')
    return myOutput

def hasErrors(output):
    '''
    Return True if the checkTrackUiLinks.csh output reports link errors.
    The script prints "No errors found!" in its summary only when no broken
    links were detected, so the absence of that line means errors were found
    (or the assembly could not be checked at all).
    '''
    return "No errors found!" not in output


def main():
    '''
    Main program to run the uiLinks cronjob for all assemblies.
    1. Get a list of all active assemblies
    2. run /cluster/bin/scripts/checkTrackUiLinks.csh
    3. Parse output
    '''
    args = parseArgs()

    # Get a list of active assemblies
    myAssemblies = getActiveAssemblies()

    # Iterate through the list of assemblies
    firstPrinted = True
    for assembly in myAssemblies:
        output = runCron(assembly)
        if os.path.exists('wget-log'):
            os.remove('wget-log')

        # In errors-only mode, skip assemblies that checked out clean
        if args.errorsOnly and not hasErrors(output):
            continue

        if not firstPrinted: # Separator for parsing the output
            print("%%%%%%%")
        print(assembly, end="\n\n")
        print(output)
        firstPrinted = False

if __name__ == '__main__':
    sys.exit(main())
