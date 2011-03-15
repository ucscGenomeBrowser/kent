import orderedDict

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
        self.add(raKey, raVal)


    def __str__(self):
        str = ''
        for key in self._ordering:
            str = str + key + ' ' + self._dictionary[key] + '\n'
        return str


