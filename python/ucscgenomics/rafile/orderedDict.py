class OrderedDict(dict):
    """
    A Dictionary ADT that preserves ordering of its keys through a parallel
    list.

    Inherits from the dict built-in python class, extending functionality 
    relevant to ordering.
    """


    def __init__(self):
        self.__ordering = list()
        dict.__init__(self)


    def __setitem__(self, key, value):
        dict.__setitem__(self, key, value)
        self.__ordering.append(key)
   
    def __delitem__(self, key):
        dict.__delitem(self, key)
        self.__ordering.remove(key)


    def __iter__(self):
        for item in self.__ordering:
            yield item


    def iterkeys(self):
        self.__iter__()


    def itervalues(self):
        for item in self.__ordering:
            yield self[item]


    def iteritems(self):
        for item in self.__ordering:
            yield item, self[item]


    def __str__(self):
        str = ''
        for item in self.iteritems():
            str += item.__str__() + '\n'
        return str

