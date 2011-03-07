import sys
import re
import orderedDict

class RaFile(orderedDict.OrderedDict):
    """
    Stores an Ra file in a set of entries, one for each stanza in the file.
    """

    def __init__(self, entryType):
       self.__entryType = entryType 
       orderedDict.OrderedDict.__init__(self)
 
    def read(self, filePath):
        """
        Reads an rafile stanza by stanza, and internalizes it.
        """

        file = open(filePath, 'r')

        entry = self.__entryType()
        stanza = list()

        for line in file:
 
            line = line.strip()

            if line.startswith('#'):
                continue

            if line != '':
                stanza.append(line)
            else:
               name = entry.readStanza(stanza)
               self[name] = entry
               entry = self.__entryType()
               stanza = list()

        file.close()


    def __str__(self):
        str = ''
        for key in self:
            str = str + self[key].__str__() + '\n'
        return str


class RaEntry(orderedDict.OrderedDict):
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

        return line.split(' ', 1)[1].strip()


    def __readLine(self, line):
        """
        Reads a single line from the stanza, extracting the key-value pair
        """

        raKey, raVal = map(str, line.split(' ', 1))
        self[raKey] = raVal


    def __str__(self):
        str = ''
        for key in self:
            str = str + key + ' ' + self[key] + '\n'
        return str

