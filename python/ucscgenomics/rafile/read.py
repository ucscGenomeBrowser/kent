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

import sys
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
            raDict.addComment(line)
            continue

        # a blank line indicates we need to move to the next entry
        if (len(line) == 0):
            raKey = None
            raEntry = None
            continue

        # check if we're at the first key in a new entry
        if (line.split()[0].strip() == keyField):
            if len(line.split()) < 2:
                print 'ERROR: blank key on <' + line + '>'
                sys.exit(1) 

            raKey = line.split(' ', 1)[1].strip()
            raEntry = radict.EntryDict()
            raEntry.add(keyField, raKey)
            raDict.add(raKey, raEntry)

        # otherwise we should be somewhere in the middle of an entry
        elif (raEntry != None):
            raKey = line.split()[0].strip()           
            raVal = ''
 
            if len(line.split()) > 1:
                raVal = line.split(' ', 1)[1].strip()

            raEntry.add(raKey, raVal)

        # we'll only get here if we didn't find the keyField at the beginning
        else:
            print 'Error: Key missing - <' + keyField + '> before line <' + line + '>.'
            return None

    file.close()
    return raDict

