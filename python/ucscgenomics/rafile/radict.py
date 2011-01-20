#
# rafile/radict.py
#
# Holds raDict class which is an internal representation of
# the data held in the RA file. Essentially just a dict that
# also has a list to preserve the computationally arbitrary
# order we want an RA entry to be in.
#

import sys

class _OrderedDict:

    def __init__(self):
        self._dictionary = dict()
        self._ordering = list()

    def add(self, key, value):
        key = key.strip()

        if (key in self._dictionary):
            print 'ERROR: RaDict.add() - Key <' + key + '> already exists'
            sys.exit(1)

        if (key == None or key == ''):
            return
        
        self._dictionary[key] = value
        self._ordering.append(key)
        
    def remove(self, key):
        key = key.strip()

        if (key not in self._dictionary):
            print 'ERROR: RaDict.remove() - Key <' + key + '> does not exist'
            sys.exit(1)

        if (key == None or key == ''):
            return

        del self._dictionary[key]
        self._ordering.remove(key)

    def getValue(self, key):
        if (key not in self._dictionary):
            print 'ERROR: RaDict.getValue() - KEY <' + key + '> does not exist'
            sys.exit(1) 

        return self._dictionary[key]

class RaDict(_OrderedDict):

    def __str__(self):
        for key in self._ordering:
            print self._dictionary[key]
        return ''

class EntryDict(_OrderedDict):

    def __str__(self):
        for key in self._ordering:
            print key + ' ' + self._dictionary[key]
        return ''
