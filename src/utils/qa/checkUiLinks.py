#!/usr/bin/python3
import subprocess
import collections
from collections import defaultdict
import sys
import os


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
    return myOutput

def runCron(assembly):
    '''
    Run the checkTrackUiLinks.csh script
    '''
    cronOutput = subprocess.run("/cluster/bin/scripts/checkTrackUiLinks.csh %s all" % assembly,\
                                         stdout=subprocess.PIPE,\
                                         stderr=subprocess.PIPE,\
                                         shell=True)
    myOutput = cronOutput.stdout.decode('utf-8')
    print(myOutput)


def main():
    '''
    Main program to run the uiLinks cronjob for all assemblies. 
    1. Get a list of all active assemblies
    2. run /cluster/bin/scripts/checkTrackUiLinks.csh
    3. Parse output
    '''
    # Get a list of active assemblies
    myAssemblies = getActiveAssemblies()
    maxCount = len(myAssemblies) -1

    # Iterate through the list of assemblies
    for count,assembly in enumerate(myAssemblies):
        print(assembly, end="\n\n")
        runCron(assembly)
        if os.path.exists('wget-log'):
            os.remove('wget-log')
        if count != maxCount: # Separator for parsing the output
            print("%%%%%%%")

if __name__ == '__main__':
    sys.exit(main())
