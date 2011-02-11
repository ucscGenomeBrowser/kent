class OrderedDict(object):
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

    def iterKeys(self):
        """
        Return an iterator over the keys in the ordering
        """

        for item in self._ordering:
            yield item

    def iterValues(self):
        """
        Return an iterator over the values in the dictionary
        """

        for item in self._ordering:
            yield self.getValue(item)

