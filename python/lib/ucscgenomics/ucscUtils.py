#!/hive/groups/encode/dcc/bin/python
import sys, os, re, argparse, subprocess, math

def isGbdbFile(file, table, database):
    errors = []
    if os.path.isfile("/gbdb/%s/bbi/%s" % (database, file)):
        return 1
    else:
        cmd = "hgsql %s -e \"select fileName from (%s)\"" % (database, table)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
        cmdoutput = p.stdout.read()
        if os.path.isfile(cmdoutput.split("\n")[1]):
            return 1
        else:
            return 0
            
def makeFileSizes(inlist, path=None):
    checklist = list()

    for i in inlist:
    	if path:
            checklist.append("%s/%s" % (path, i))    
        else:
            checklist.append(i)
    filesizes = 0
    for i in checklist:
        realpath = os.path.realpath(i)
        filesizes = filesizes + int(os.path.getsize(realpath))

    filesizes = math.ceil(float(filesizes) / (1024**2))

    return int(filesizes)
    
def printIter(set, path=None):
    output = []
    for i in sorted(set):
        if path:
            output.append("%s/%s" % (path, i))
        else:
            output.append("%s" % (i))
    return output