#!/usr/bin/env python2.7
# softToTagStorm
# Chris Eisenhart 07/14/2015 
# ceisenha@ucsc.edu/ceisenhart@soe.ucsc.edu
"""
Convert from soft format to tagStorm format. 
"""

from __future__ import print_function  
import  sys, operator, fileinput, collections, string, os.path
import  re, argparse, subprocess



specialTags = ["!Sample_characteristics_"]


# functions 
def warning(*objs):
    print("WARNING: ", *objs, file=sys.stderr)
 
def error(*objs):
    """
    Quit the program, if there is an error message spit it to stderr. 
    """
    print("ERROR: ", *objs, file=sys.stderr)
    sys.exit()

def parseArgs(args): 
    """
    Parse the arguments into an opened file for reading (inputFile), and 
    an open file for writing (outputFile). 
    """
    parser = argparse.ArgumentParser(description = __doc__)
    parser.add_argument ("inputFile",
    help = " Specifies that the text should be read in from an"
		" input file. ",
    type = argparse.FileType('r'))
    parser.add_argument ("outputFile",
    help = " Specifies that the word and count pairs should"
		" be printed to an output file. ",
    type = argparse.FileType('w'))
    parser.add_argument ("--tagHash",
    help = " The user provides a hash table that maps soft names to user supplied "
		" tags.",
    type = argparse.FileType('r'))
    parser.add_argument ("--strict",
    help = " The program will abort if a tag is encountered that is not in the  "
                " user supplied tag file. Only works with the --tagHash option. ", 
    action = "store_true")
    parser.add_argument ("--verbose",
    help = " Spit error messages. ",
    action = "store_true")
    parser.set_defaults (strict = False)
    parser.set_defaults (verbose = False)
    parser.set_defaults (tagHash = None)
    options = parser.parse_args() # Options is a structure that holds the command line arguments information.
    return options


def main(args):
    """
    Converts soft files to tagstorm files.  Goes through the soft file looking for
    hierarchy tags.  The lines in between these tags are stored in a buffer. When
    a new tag is encountered the depth is updated and the buffer is flushed.  
    """
    options = parseArgs(args)
    inputFile = options.inputFile
    outputFile = options.outputFile
    depth = 0   # The current depth. 
    currentStanza = None
    firstLine = True
    tagHashTable = dict()
    if options.tagHash !=None: 
        for line in options.tagHash: 
            splitLine = line.split()
            tagHashTable.setdefault(splitLine[0], splitLine[1])
    lineCount = 0
    buffer = dict()
    for line in inputFile:
        lineCount += 1
        splitLine = line.split('=')
        nameTag = splitLine[0][1:].replace(" ","").strip("\n") # Strip the '!' and any white space, store this as the name tag
        valTag = splitLine[1].strip("\n").strip(" ")
        for item in splitLine[2:]: # Gather the rest of the string, this is the value tag
            valTag += "=" + item
        valTag = valTag.replace("\n","")
        if options.tagHash != None: # Check if we are providing our own tags
            if splitLine[0][1:-1] not in tagHashTable: 
                if (not options.strict) and options.verbose: 
                    warning("The tag %s in the soft file on line %i was not found in the provided hashTable."
                            "The program is using the native tags."%(splitLine[0][1:-1], lineCount))

                if options.strict: 
                    error("The tag %s in the soft file on line %i was not found in the provided hashTable."
                            " The program is aborting."%(splitLine[0][1:-1], lineCount))
                    continue 

        if line.startswith("!"):    # Handle all general tags
            if ("!" + nameTag) in tagHashTable: # We have our own version of this tag
                buffer.setdefault(tagHashTable["!" + nameTag],valTag)                
            else:
                if splitLine[0].startswith("!Sample_characteristics_"):
                    subTags = splitLine[1].strip("\n").split(":")
                    buffer.setdefault("GEO_Sample_"+subTags[0][1:].replace(" ","_"), subTags[1])
                    continue 

                if buffer.get(nameTag): buffer[nameTag] +=  ", " + valTag[:-1]
                else: buffer.setdefault("GEO_"+nameTag,valTag)

        if line.startswith('^'):    # Look for new stanzas and flush the previous data. 
            if firstLine:
                firstLine = False
                if nameTag in tagHashTable :  outputFile.write(tagHashTable[nameTag] + "\t" +  valTag + "\n")
                else:  outputFile.write("GEO_"+ nameTag + "\t" +  valTag + "\n")
                continue
            
            for key,val in buffer.iteritems(): 
                outputLine = ("%s\t%s\n"%(key,val.strip("\n")))          
                outputFile.write(depth * "\t" + outputLine)
            
            if splitLine[0] != currentStanza:   # Only update depth when a new stanza is seen 
                currentStanza = splitLine[0]
                depth += 1  

            buffer = dict()
            outputFile.write("\n")
            if nameTag in tagHashTable: outputFile.write(depth*"\t" + tagHashTable.get(nameTag) + "\t" +  valTag + "\n")
            else: outputFile.write(depth*"\t" + "GEO_" + nameTag + "\t" +  valTag + "\n")

    for key,val in buffer.iteritems(): # Flush the final buffer. 
        outputLine = ("%s\t%s\n"%(key,val))          
        outputFile.write(depth * "\t" + outputLine)

if __name__ == "__main__" :
    sys.exit(main(sys.argv))

