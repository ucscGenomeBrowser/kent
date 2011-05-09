import sys
import re
from CvRaFile import *

__ra = CvRaFile(sys.argv[1])
__groupDict = dict()
__filename = sys.argv[1]


def KeyEquals(key, matchBehavior, group):
    """
    Matches entries containing the specified key.
    """
    for item in __ra.itervalues(): 
        if key in item.keys():
            matchBehavior(item.name, group)


def KeyMatches(regex, matchBehavior, group):
    """
    Matches entries with a key that matches the regular expression.
    """
    for item in __ra.itervalues():
        for key in item.iterkeys():
            if re.match(regex, key):
                matchBehavior(item.name, group)
                break


def ValueEquals(key, value, matchBehavior, group):
    """
    Matches entries with the specified key-value pair
    """
    for item in __ra.itervalues():
        if item[key] == value:
            matchBehavior(item.name, group)

    
def ValueMatches(key, regex, matchBehavior, group):
    """
    Matches entries with a key-value pair that regex matches the value.
    """
    for item in __ra.itervalues():
        if re.match(regex, item[key]):
            matchBehavior(item.name, group)


def doNothing(key, group):
    """
    Match behavior that does nothing.
    """
    pass


def addToGroup(key, group):
    """
    Match behavior that adds a matched entry to a group.
    """
    if group not in __groupDict:
        __groupDict[group] = list()
    if key not in __groupDict[group]:  
        __groupDict[group].append(key) 


def removeFromGroup(key, group):
    """
    Match behavior that removes a matched entry from a group.
    """
    if group in __groupDict:
        if key in __groupDict[group]:
            __groupDict[group].remove(key)


def PrintResult():
    """
    Prints out each group and the entries it has matched.
    """
    for key in __groupDict.iterkeys():
        print key + ':'
        for val in __groupDict[key]:
            print val
