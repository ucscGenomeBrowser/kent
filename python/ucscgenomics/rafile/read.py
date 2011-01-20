#
# rafile/read.py
# 
# takes in an RA file and creates a dictionary structure for each one. Then puts
# all entries into 1 dictionary, which is indexed by the value of the key-value 
# pair specified by keyField (typically the object's name) which has to be the
# first line in each entry. An example input with its created raDict follows.
#
# EXAMPLE:
#
# INPUT:
#     readRaFile(file, key1)
#
#     file:     
#         key1 valueA
#         key2 valueB
#
#         key1 valueC
#         key2 valueD
# 
# OUTPUT:
#     raDict
#         value1
#             key1 valueA
#             key2 valueB
#         value3
#             key1 valueC
#             key2 valueD
#
#
#
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

        # remove all commented lines
        if (line.startswith('#')):
            continue

        # a blank line indicates we need to move to the next entry
        if (len(line) == 0):
            raKey = None
            raEntry = None
            continue

        # check if we're at the first key in a new entry
        if (line.split()[0] == keyField):
            raKey = line.split()[1]
            raEntry = radict.EntryDict()
            raEntry.add(keyField, raKey)
            raDict.add(raKey, raEntry)

        # otherwise we should be somewhere in the middle of an entry
        elif (raEntry != None):
            splits = line.split()            
            raEntry.add(splits[0], splits[1])

        # we'll only get here if we didn't find the keyField at the beginning
        else:
            print 'Error: Key missing - <' + keyField + '> before line <' + line + '>.'
            return None

    file.close()
    return raDict

