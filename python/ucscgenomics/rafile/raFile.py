import sys
import re

class _OrderedDict(object):
    """
    Abstract class containing all shared functionality between the RaFile and
    RaEntry classes.

    Contains a dictionary to hold each entry, as well as a list to hold the
    computationally arbitrary ordering we wish to preserve between reads and
    writes.  
    """

    def __init__(self):
        self._dictionary = dict()
        self._ordering = list()

    def add(self, key, value):
        """
        Add a key-value pair to the dictionary, and put its key in the list.
        """

        key = key.strip()

        if (key in self._dictionary):
            raise KeyError()
            sys.exit(1)

        if (key == None or key == ''):
            return
        
        self._dictionary[key] = value
        self._ordering.append(key)
        
    def remove(self, key):
        """
        Remove a key-value pair from the dictionary, and the key from the list.
        """

        key = key.strip()

        if (key not in self._dictionary):
            raise KeyError()

        if (key == None or key == ''):
            return

        del self._dictionary[key]
        self._ordering.remove(key)

    def getValue(self, key):
        """
        Return the value associated with a key in the dictionary.
        """

        if (key not in self._dictionary):
            return None

        return self._dictionary[key]

    def getKeyAt(self, index):
        """
        Return the key associated with the index in this list
        """

        if (index > len(self._ordering)):
            raise IndexError()

        return self._ordering[index]

    def getValueAt(self, index):
        """
        Return the value associated with an index in the list.
        """

        if (index > len(self._ordering)):
            raise IndexError()

        return self._dictionary[self._ordering[index]]

    def count(self):
        """
        Return the length of the list.
        """

        return len(self._ordering)


class RaFile(_OrderedDict):
    """
    Stores an Ra file in a set of entries, one for each stanza in the file.
    """

    def read(self, filePath, keyField):
        """
        Reads an rafile, separating it by keyField, and internalizes it.

        keyField must be the first field in each entry.
        """

        file = open(filePath, 'r')
        raEntry = RaEntry()
        raKey = None

        for line in file:

            line = line.strip()

            # put all commented lines in the list only
            if (line.startswith('#')):
                self._ordering.append(line)
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
        """
        Write out the entire RaFile.
        """

        print self

    def writeEntry(self, key):
        """
        Write out a single stanza, specified by key
        """

        print self.getValue(key)

    def __iter__(self):
        return self

    def __str__(self):
        str = ''
        for key in self._ordering:
            if key.startswith('#'):
                str = str + key + '\n'
            else:
                str = str + self._dictionary[key].__str__() + '\n'
        return str

class RaEntry(_OrderedDict):
    """
    Holds an individual entry in the RaFile.
    """

    def __init__(self):
        

    def next(self):
        

    def __iter__(self):
        return self

    def __str__(self):
        str = ''
        for key in self._ordering:
            str = str + key + ' ' + self._dictionary[key] + '\n'
        return str
