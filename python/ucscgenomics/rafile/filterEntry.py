import sys
import re

class FilterEntry(object):

    def __init__(self):
        self._clauses = list()
        self._match = list()

    def add(self, key, value):
        if key == '_add':
            self._match.append([value, 'add'])
        if key == '_remove':
            self._match.append([value, 'remove'])  
        else:
            self._clauses.append([key, value])

    def getKeyAt(self, index):
        return self._clauses[index][0]

    def getValueAt(self, index):
        return self._clauses[index][1]

    def getValue(self, key):
        for entry in self._clauses:
            if entry[0] == key:
                return entry[1]
        return None

    def count(self):
        return len(self._clauses)

    def iterKeys(self):
        for item in self._clauses:
            yield item[0]

    def iterValues(self):
        for item in self._clauses:
            yield item[1]

    def iterMatches(self):
        for item in self._match:
            yield item

    def iterMatchValues(self):
        for item in self._match:
            yield item[1]


