#!/usr/bin/env python2.7

import logging, sys, optparse, string
from collections import defaultdict
import os
from os.path import join, basename, dirname, isfile

# ==== functions =====
    
def parseArgs():
    " setup logging, parse command line arguments and options. -h shows auto-generated help page "
    parser = optparse.OptionParser("usage: %prog [options] inTabFile inBedFile outBedFile - given a bed file and tab file where each have a column with matching values: first get the value of column0, the offset and line length from inTabFile. Then go over the bed file, use the name field and append its offset and length to the bed file as two separate fields. Write the new bed file to outBed.")

    parser.add_option("-d", "--debug", dest="debug", action="store_true", help="show debug messages")
    parser.add_option("-t", "--tabKeyField", dest="tabKeyField", action="store", type="int", \
        help="the index of the key field in the tab file that matches the key field in the bed file. default %default", default=0)
    parser.add_option("-b", "--bedKeyField", dest="bedKeyField", action="store", type="int", \
        help="the index of the key field in the bed file that matches the key field in the tab file. default %default", default=3)
    #parser.add_option("", "--headers", dest="headers", action="store", help="comma-separated list of headers for outFile")
    #parser.add_option("", "--test", dest="test", action="store_true", help="do something") 
    (options, args) = parser.parse_args()

    if args==[]:
        parser.print_help()
        exit(1)

    if options.debug:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)
    return args, options

def iterLineOffsets(ifh):
    """ parse a text file and yield tuples of (line, startOffset, endOffset).
    endOffset does include the newline
    """
    line = True
    start = 0
    while line!='':
       line = ifh.readline()
       end = ifh.tell()
       if line!="":
           yield line, start, end
       start = ifh.tell()

def tabOffsets(fnames, keyCol):
    " iterate over all lines in tab filenames and return dict with value -> (offset, lineLen) "
    idxFnames = {}
    ret = {}
    for i, fname in enumerate(fnames):
        print ("Reading %s, %0.f %%"  % (fname, float(100*i)/len(fnames)))
        offset = 0
        for line, start, end in iterLineOffsets(open(fname, "rb")):
            if keyCol==0:
                # most common case: saves a bit of time by not building a huge list
                fields = string.split(line, "\t", 1)
                if len(fields)==1:
                    raise Exception("file %s does not seem to be a tab-separated file" % fname)
                key, restLine = fields
            else:
                key = line.rstrip("\n").split("\t")[keyCol]

            if key in idxFnames:
                print("Warning: duplicate key %s, found in files %s and %s" % \
                    (key, fname, idxFnames[key]))
                continue
            idxFnames[key] = fname

            ret[key] = (start, len(line))

    return ret

def bedJoinTabOffset(inTab, inBed, outBed, options):
    " cat and index all files in inDir "

    #if isdir(inTab):
    #print ("Getting files in directory %s" % inDir)
    #fnames = os.listdir(inDir)
    #print ("Found %d files" % len(fnames))

    keyToOffLen = tabOffsets([inTab], options.tabKeyField)
    allKeys = set(keyToOffLen)

    bedOfh = open(outBed, "wb")

    keyIdx = options.bedKeyField
    for line in open(inBed):
        if line=="\n":
            continue
        fields = line.rstrip("\n").split("\t")
        key = fields[keyIdx]
        offLen = keyToOffLen.get(key)
        if offLen == None:
           logging.error("bed key '%s' is not in the tab-sep file" % key)
           assert(False)
        offset, lineLen = offLen
        newLine = "%s\t%s\t%s\n" % (line.rstrip("\n"), offset, lineLen)
        bedOfh.write(newLine)
        if key in allKeys:
            allKeys.remove(key)

    bedOfh.close()
    logging.info("Wrote %s" % outBed)

    if len(allKeys)!=0:
        logging.warn("The following keys in the tab file were not referenced in the bed file: %s" % ", ".join(allKeys))

# ----------- main --------------
def main():
    args, options = parseArgs()
    inTab, inBed, outBed = args
    bedJoinTabOffset(inTab, inBed, outBed, options)

main()
