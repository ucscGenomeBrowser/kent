import orderedDict

class RaEntry(orderedDict.OrderedDict):
    """
    Holds an individual entry in the RaFile.
    """

    def __str__(self):
        str = ''
        for key in self._ordering:
            str = str + key + ' ' + self._dictionary[key] + '\n'
        return str
