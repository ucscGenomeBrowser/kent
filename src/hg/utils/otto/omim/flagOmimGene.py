#!/usr/bin/env python3

import argparse
import subprocess
import sys

parser = argparse.ArgumentParser(description="Print rows from omimGene2 that lose chromosomes in new version of table.\nWrites records to stdout. Intended usage:\nflagOmimGene.py hg19 > dateBackupOmimGene2.txt", formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("db", help="ucsc database name")
parser.add_argument("-v", "--verbose", action="store_true", help="Print out verbose messages about number of changed records")

if len(sys.argv) == 1:
    parser.print_help()
    sys.exit(1)

args = parser.parse_args()

# build up new dict and old dict and then compare
oldMap = {}
newMap = {}

def verbose(message):
    if args.verbose:
        print(message)

def makeDictFromTable(inDict, tblName, dbName):
    cmd = "hgsql -Ne \"select * from %s\" %s" % (tblName, dbName)
    try:
        out = subprocess.check_output(cmd, universal_newlines=True, shell=True)
        for line in out.strip().split("\n"):
            l = line.split()
            if len(l) < 5:
                print("bad line: %s" % line)
                sys.exit(1)
            chrom = l[1]
            name = l[4]
            try:
                inDict[name].append(chrom)
            except KeyError:
                inDict[name] = [chrom]
        return inDict
    except subprocess.CalledProcessError as e:
        print(e.output)
        sys.exit(e.returncode)

oldMap = makeDictFromTable(oldMap, "omimGene2", args.db)
newMap = makeDictFromTable(newMap, "omimGene2New", args.db)

oldCount = len(oldMap)
newCount = len(newMap)
verbose("%d ids in new release" % newCount)
verbose("%d ids in old release" % oldCount)
if newCount > oldCount:
    verbose("%d ids added to this release" % (newCount - oldCount))
elif newCount < oldCount:
    verbose("%d ids dropped in this release" % (oldCount - newCount))
else:
    verbose("same number ids both releases")

uniqNew = {k for k in newMap if k not in oldMap}
uniqOld = {k for k in oldMap if k not in newMap}
sharedKeys = set(newMap.keys()) & set(oldMap.keys())
sharedKeyCount = len(sharedKeys)
diffItems = {k:v for k,v in newMap.items() if k in oldMap and v != oldMap[k]}


verbose("%d / %d ids unique to new release" % (len(uniqNew), newCount))
verbose("%d / %d ids unique to old release" % (len(uniqOld), oldCount))
verbose("%d ids shared between releases" % sharedKeyCount)
verbose("%d / %d  ids exist in both releases but differ" % (len(diffItems.keys()), sharedKeyCount))

suspectIds = []
if len(diffItems) > 0:
    verbose("comparing different values between ids:")
    for key in diffItems:
        newChromList = set(newMap[key])
        oldChromList = set(oldMap[key])
        uniqNewChroms = newChromList - oldChromList
        uniqOldChroms = oldChromList - newChromList
        common = newChromList & oldChromList
        if uniqOldChroms:
            suspectIds.append(key)
            verbose("omim id:%s - chroms shared: %s - "
            "unique old chroms: %s - unique new chroms:%s" % 
            (key, list(common), list(uniqOldChroms), list(uniqNewChroms) )) 
    if len(suspectIds) > 0:
        for omimId in suspectIds:
            cmd = "hgsql -Ne \"select chrom, chromStart, chromEnd, name from omimGene2 where name='%s'\" %s" % (omimId, args.db)
            out = subprocess.check_output(cmd, universal_newlines=True, shell=True)
            print(out.strip())
else:
    verbose("no difference in shared ids")
