#
# rafile/read.py
# 
# PURPOSE UNCLEAR
#

import re
import radict

def readRaFile(filePath, keyField):

    file = open(filePath, 'r')
    raDict = radict.RaDict()
    raEntry = radict.EntryDict()
    raKey = None

    for line in file:

        line = line.strip()

        if (line.startswith('#')):
            continue

        if (len(line) == 0):
            raKey = None
            raEntry = None
            continue

        if (line.split()[0] == keyField):
            raKey = line.split()[1]
            raEntry = radict.EntryDict()
            raEntry.add(keyField, raKey)
            raDict.add(raKey, raEntry)

        elif (raEntry != None):
            splits = line.split()            
            raEntry.add(splits[0], splits[1])

        else:
            print 'Error: Key missing - <' + keyField + '> before line <' + line + '>.'
            return None

    file.close()
    return raDict

