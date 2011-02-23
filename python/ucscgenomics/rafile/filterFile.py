import sys
import re
import orderedDict

class FilterFile(orderedDict.OrderedDict):
    """
    Stores an Ra filter in a list of critera.
    """

    Match = list()

    def read(self, filePath, keyField):
        """
        Reads an rafile, separating it by keyField, and internalizes it.

        keyField must be the first field in each entry.
        """

        file = open(filePath, 'r')
        entry = list()
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
                entry = None
                continue

            # check if we're at the first key in a new entry
            if (line.split()[0].strip() == keyField):
                if len(line.split()) < 2:
                    raise KeyError()

                raKey = line.split(' ', 1)[1].strip()
                entry = list()
                entry.append([keyField, raKey])
                print entry
                self.add(raKey, entry)

            # otherwise we should be somewhere in the middle of an entry
            elif (entry != None):
                raKey = line.split()[0].strip()
                raVal = ''

                if len(line.split()) > 1:
                    raVal = line.split(' ', 1)[1].strip()

                if raKey == '_add':
                    self.Match.append([raVal, 'add'])
                if raKey == '_remove':
                    self.Match.append([raVal, 'remove'])                

                entry.append([raKey, raVal])
                print entry

            # we only get here if we didn't find the keyField at the beginning
            else:
                raise KeyError()

        file.close()

    def getKeyAt(self, index):
        return self.getValue(self._ordering[index])

    def getValueAt(self, index):
        return self.getValue(self._ordering[index])

    def count(self):
        return len(self._ordering)

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

    def __str__(self):
        str = ''
        for key in self._ordering:
            if key.startswith('#'):
                str = str + key + '\n'
            else:
                str = str + self._dictionary[key].__str__() + '\n'
        return str
