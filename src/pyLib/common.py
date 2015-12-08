#!/usr/bin/env python2.7
"""
This is a python module that holds common/frequently used functions (C header + C library equivalent). 
Using these functions; consider a program named foo...

#!/usr/bin/env python2.7

import common

common.warning("This program has no function") 

This will run the common.py function cloneProgToPath with the variable 'program'. 
"""
import sys, string, operator, fileinput, collections, os.path, time
import re, argparse, math, tempfile, subprocess, getpass, inspect
import smtplib, MySQLdb, itertools

def dictFromTwoTabFile(fileName, hasHeader):   
    """
    Input:
        fileName - a string
        hasHeader - a bool
    Output:
        hash - A dict, string keys map to string values.  
    Takes in a string that pointes to a two column file. The file is opened and parsed into a 
    dict. If the hasHeader flag is selected then the first line is skipped.
    """
    hash = dict()
    lineNum = 1 
    for line in open(fileName,"r"):
        if (hasHeader):
            hasHeader = False
            continue
        splitLine = line.split()
        if (hash.get(line[-1]) == None):    
            hash.setdefault(splitLine[0], splitLine[1])
        else: print("A duplicate name was found on line %i, please fix the file or use a "
                        "different data structure."%(lineNum))
        lineNum += 1
    return hash

def readInTable(file, keyColumn, separator): 
    """
    Input:
        file - an opened file
        keyColumn - an integer
        separator - a string
    Output:
        result - A dict, string keys map to list of string values.
    Process a table into a hash of lists. Each row in the table will be stored as a hash
    entry, one field will act as a key. The key column indicates which values should
    key the hash table. The remaining elements are stored in a list based on their location. 
    """
    result = dict()
    firstLine = True
    for line in file:
        if firstLine:
            firstLine = False
            continue
        
        splitLine = line[:-1].split(separator)
        count = 0
        list = []
        for item in splitLine: 
            list.append(item)
        result.setdefault(splitLine[keyColumn],list)
    return result 

def parseConf(fname):
    """ parse a hg.conf style file,
        return a dict key -> value (both are strings)
    """

    conf = {}
    for line in open(fname):
        line = line.strip()
        if line.startswith("#"):
            continue
        elif line.startswith("include "):
            inclFname = line.split()[1]
            inclPath = join(dirname(fname), inclFname)
            if isfile(inclPath):
                inclDict = parseConf(inclPath)
                conf.update(inclDict)
        elif "=" in line: # string search for "="
            key, value = line.split("=")
            conf[key] = value
    return conf

hgConf = None

def parseHgConf(confDir="."):
    """ return hg.conf as dict key:value """
    global hgConf
    if hgConf is not None:
        return hgConf

    hgConf = dict() # python dict = hash table
    fname = join(currDir, confDir, "hg.conf")
    if not isfile(fname):
        return {}
    hgConf = parseConf(fname)

    return hgConf

def getSQLLoginInfo():
    """
    Output: 
        result - An object with three elements, host, user, password.
    Grab the SQL login info from the users home directory hg.conf file. 
    """
    hst = ""
    usr = ""
    pw = ""
    for line in open("/cluster/home/" + getpass.getuser() + "/.hg.conf","r"):
        splitLine=line.split("=")
        if (splitLine[0]== "db.host"): hst = splitLine[1][:-1]
        if (splitLine[0]== "db.user"): usr = splitLine[1][:-1]
        if (splitLine[0]== "db.password"): pw = splitLine[1][:-1]
    return (hst, usr, pw)


def sqlQuery(database, query): 
    """
    This is still being tested out!!! If for some reason your using this (and your not Chris Eisenhart)
    be EXTREMELY careful. This has no Injection prevention yet, and will directly modify the table. You
    probably aught not to be doing this...
    """
    loginCredentials = getSQLLoginInfo()
    db = MySQLdb.connect(host= loginCredentials[0],user = loginCredentials[1], passwd = loginCredentials[2], db = database)
    cur = db.cursor()
    cur.execute(query)
    return cur 

