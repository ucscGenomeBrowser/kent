#
# Holds raDict class which is an internal representation of
# the data held in the RA file. Essentially just a dict that
# also has a list to preserve the computationally arbitrary
# order we want an RA entry to be in.
#

import sys
import re

class _OrderedDict(object):

    def __init__(self):
        self._dictionary = dict()
        self._ordering = list()

    def add(self, key, value):
        key = key.strip()

        if (key in self._dictionary):
            raise KeyError()
            sys.exit(1)

        if (key == None or key == ''):
            return
        
        self._dictionary[key] = value
        self._ordering.append(key)
        
    def remove(self, key):
        key = key.strip()

        if (key not in self._dictionary):
            raise KeyError()

        if (key == None or key == ''):
            return

        del self._dictionary[key]
        self._ordering.remove(key)

    def getValue(self, key):
        if (key not in self._dictionary):
            raise KeyError()

        return self._dictionary[key]

class RaFile(_OrderedDict):

    def addComment(self, comment):
        if not comment.startswith('#'):
            raise Exception()

        self._ordering.append(comment)

    def read(self, filePath, keyField):

        file = open(filePath, 'r')
        raEntry = RaEntry()
        raKey = None

        for line in file:

            line = line.strip()

            # remove all commented lines
            if (line.startswith('#')):
                self.addComment(line)
                continue

            # a blank line indicates we need to move to the next entry
            if (len(line) == 0):
                raKey = None
                raEntry = None
                continue

            # check if we're at the first key in a new entry
            if (line.split()[0].strip() == keyField):
                if len(line.split()) < 2:
                    raise KeyError()

                raKey = line.split(' ', 1)[1].strip()
                raEntry = RaEntry()
                raEntry.add(keyField, raKey)
                self.add(raKey, raEntry)

            # otherwise we should be somewhere in the middle of an entry
            elif (raEntry != None):
                raKey = line.split()[0].strip()
                raVal = ''

                if len(line.split()) > 1:
                    raVal = line.split(' ', 1)[1].strip()

                raEntry.add(raKey, raVal)

            # we only get here if we didn't find the keyField at the beginning
            else:
                raise KeyError()

        file.close()

    def write(self):
        print self

    def writeEntry(self, key):
        print self.getValue(key)

    def __str__(self):
        str = ''
        for key in self._ordering:
            if key.startswith('#'):
                str = str + key + '\n'
            else:
                str = str + self._dictionary[key].__str__() + '\n'
        return str

class RaEntry(_OrderedDict):

    def __str__(self):
        str = ''
        for key in self._ordering:
            str = str + key + ' ' + self._dictionary[key] + '\n'
        return str
