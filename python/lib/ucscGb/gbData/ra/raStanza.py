import sys, string
import re
from ucscGb.gbData.ordereddict import OrderedDict
from ucscGb.gbData import ucscUtils
import collections

class RaStanza(OrderedDict):
    '''
    Holds an individual entry in the RaFile.
    '''

    @property
    def name(self):
        return self._name

    def __init__(self):
        self._name = ''
        self._nametype = ''
        OrderedDict.__init__(self)

    def readStanza(self, stanza, key=None):
        '''
        Populates this entry from a single stanza. Override this to create
        custom behavior in derived classes
        '''

        for line in stanza:
            self.readLine(line)

        return self.readName(stanza, key)


    def readName(self, stanza, key=None):
        '''
        Extracts the Stanza's name from the value of the first line of the
        stanza.
        '''
        
        if key == None:
            line = stanza[0]
        else:
            line = None
            for s in stanza:
                if s.split(' ', 1)[0] == key:
                    line = s
                    break
            if line == None:
                return None
        
        if len(line.split(' ', 1)) != 2:
            raise ValueError()

        names = map(str.strip, line.split(' ', 1))
        self._nametype = names[0]
        self._name = names[1]
        return names

    def readLine(self, line):
        '''
        Reads a single line from the stanza, extracting the key-value pair
        '''

        if line.startswith('#') or line == '':
            OrderedDict.append(self, line)
        else:
            raKey = line.split(' ', 1)[0]
            raVal = ''
            if (len(line.split(' ', 1)) == 2):
                raVal = line.split(' ', 1)[1]
            #if raKey in self:
                #raise KeyError(raKey + ' already exists')
            self[raKey] = raVal

    def difference(self, other):
        '''
        Complement function to summaryDiff.
        Takes in self and a comparison Stanza.
        Returns new stanza with terms from 'self' that are different from 'other'
        Like the summaryDiff, to get the other terms, this needs to be run
        again with self and other switched.
        '''
        retRa = RaStanza()
        retRa._name = self.name
        for key in other.keys():
            try:
                if other[key] != self[key] and not key.startswith('#'):
                    retRa[key] = self[key]
            except KeyError:
                continue
                #maybe add empty keys
        return retRa

    def iterkeys(self):
        for item in self._OrderedDict__ordering:
            if not (item.startswith('#') or item == ''):
                yield item


    def itervalues(self):
        for item in self._OrderedDict__ordering:
            if not (item.startswith('#') or item == ''):
                yield self[item]


    def iteritems(self):
        for item in self._OrderedDict__ordering:
            if not (item.startswith('#') or item == ''):
                yield item, self[item]


    def iter(self):
        iterkeys(self)

        
    def __str__(self):
        str = ''
        for key in self:
            if key.startswith('#'):
                str += key + '\n'
            else:
                str += key + ' ' + self[key] + '\n'

        return str

