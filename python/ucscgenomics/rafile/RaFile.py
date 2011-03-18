import sys
import re
import OrderedDict

class RaFile(OrderedDict.OrderedDict):
    """
    Stores an Ra file in a set of entries, one for each stanza in the file.
    """

    def __init__(self, entryType):
       self.__entryType = entryType 
       OrderedDict.OrderedDict.__init__(self)
 
    def read(self, filePath):
        """
        Reads an rafile stanza by stanza, and internalizes it.
        """

        file = open(filePath, 'r')

        entry = self.__entryType()
        stanza = list()
        keyValue = ''

        for line in file:
 
            line = line.strip()

            if len(stanza) == 0 and line.startswith('#'):
                self._OrderedDict__ordering.append(line)
                continue

            if line != '':
                stanza.append(line)
            elif len(stanza) > 0:
               if keyValue == '':
                   keyValue, name = entry.readStanza(stanza)
               else:
                   testKey, name = entry.readStanza(stanza)
                   if keyValue != testKey:
                       raise KeyError('Inconsistent Key ' + testKey)
       
               if name in self:
                   raise KeyError('Duplicate Key ' + name)

               self[name] = entry
               entry = self.__entryType()
               stanza = list()

        if len(stanza) > 0:
            raise IOError('File is not newline terminated')

        file.close()


    def itervalues(self):
        for items in self._OrderedDict.__ordering(self):
            if not item.startswith('#'):
                yield self[item]


    def iteritems(self):
        for item in self._OrderedDict__ordering:
            if not item.startswith('#'):
                yield item, self[item]
            else:
                yield [item]


    def __str__(self):
        str = ''
        for item in self.iteritems():
            if len(item) == 1:
                str += item[0].__str__() + '\n'
            else:
                str += item[1].__str__() + '\n'
        return str


class RaEntry(OrderedDict.OrderedDict):
    """
    Holds an individual entry in the RaFile.
    """

    def readStanza(self, stanza):
        """
        Populates this entry from a single stanza
        """

        for line in stanza:
            self.__readLine(line)

        return self.__readName(stanza[0])


    def __readName(self, line):
        """
        Extracts the Stanza's name from the value of the first line of the
        stanza.
        """

        if len(line.split(' ', 1)) != 2:
            raise ValueError()

        return map(str.strip, line.split(' ', 1))


    def __readLine(self, line):
        """
        Reads a single line from the stanza, extracting the key-value pair
        """ 

        if line.startswith('#'):
            raise KeyError('Comment in the middle of a stanza')

        raKey, raVal = map(str, line.split(' ', 1))
        self[raKey] = raVal


    def __str__(self):
        str = ''
        for key in self:
            str = str + key + ' ' + self[key] + '\n'
        return str

