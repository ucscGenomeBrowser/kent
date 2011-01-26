#
# rafile/write.py
#
# Handles the writing of the raDict object. Mostly just wrapper functions for
# the class's tostring method.
#

import sys
import radict

def writeRaFile(raDict, *args):
    
    if not isinstance(raDict, radict.RaDict):
        print 'ERROR: writeRaFile() - invalid raDict argument'
        sys.exit(1)

    keys = map(str, args)

    # if specific keys supplied, print out the stanzas associated with them
    if len(keys) > 0: 
        for key in keys:
            print raDict.getValue(key)

    #otherwise just print the whole raDict
    else:
        print raDict

