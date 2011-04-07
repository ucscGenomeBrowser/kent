import sys
import re
from RaFile import RaFile, RaEntry

__ra = RaFile(RaEntry)
__groupDict = dict()
__filename = sys.argv[1]
__ra.read(__filename)

def EntryExists(key, matchBehavior, group):
    if key in __ra.keys():
        matchBehavior(key, group)


def KeyEquals(key, matchBehavior, group):
    for item in __ra.itervalues(): 
        if key in item.keys():
            matchBehavior(item.name, group)


def KeyMatches(regex, matchBehavior, group):
    for item in __ra.itervalues():
        for key in item.iterkeys():
            if re.match(regex, key):
                matchBehavior(item.name, group)
                break


def ValueEquals(key, value, matchBehavior, group):
    for item in __ra.itervalues():
        if item[key] == value:
            matchBehavior(item.name, group)

    
def ValueMatches(key, regex, matchBehavior, group):
    for item in __ra.itervalues():
        if re.match(regex, item[key]):
            matchBehavior(item.name, group)


def doNothing(key, group):
    pass


def addToGroup(key, group):
    if group not in __groupDict:
        __groupDict[group] = list()
    if key not in __groupDict[group]:  
        __groupDict[group].append(key) 


def removeFromGroup(key, group):
    if group in __groupDict:
        if key in __groupDict[group]:
            __groupDict[group].remove(key)


def PrintResult():
    for key in __groupDict.iterkeys():
        print key + ':'
        for val in __groupDict[key]:
            print val
