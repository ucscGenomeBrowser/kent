#!/usr/bin/env python2.7
# jsonToTagStorm.py
# Chris Eisenhart 03/24/2015
"""
This code convertes a single line (condensed) json file into a tagStorm file. 
The program can be run as follows; jsonToTagStorm.py < input.json > output.tagStorm 
"""
from __future__ import print_function  
import  sys, operator, fileinput, collections, string, os.path
import  re, argparse, random

def parseArgs(args): 
    """
    Set the specifications for user provided options. The following 
    options are supported, 
    verbose
    """
    parser = argparse.ArgumentParser(description = __doc__)
    parser.add_argument ("--verbose", "-v",
    help = " Print individual simulation statistics and all default conditions",
    action = "store_true")
    parser.set_defaults (verbose = False)
    options = parser.parse_args()
    return options


def prettyPrint(name, value, depth):
    """
    Prints the name value tag to the correct depth. 
    """
    space = depth * '    '
    if (depth > 0):print(space[1:],name,value)
    else: print (name,value)

def main(args):
    """
    Reads in the user commands and options, opening
    any files if necessary otherwise reading from sys.stdin. 
    """
    options = parseArgs(args)
    if options.verbose:
        print(options.verbose, options.columnOrder)
    depth = 0
    # The starting depth is always 0, it is updated keeping track of how far the stanza needs to be indented
    for bigLine in sys.stdin:
        #This loop only runs one time, it grabs the first and only line of the input. 
	line = bigLine[:-1].replace(" ","") # Remove newline and spaces
	splitByArrays = line.split("[") # Split the string apart on .json array openings
    	arrays = list(splitByArrays)
        arrays.pop(0) # The first stanza is a placeholder
	for item in arrays: 
	    # This iterates over the various .json arrays, inside the arrays are split 
	    # by open brackets, this produces a tagStorm stanza on a single line with unecessary punctuation
            tagStormStanzas = item.split("{")
	    stanzas = list(tagStormStanzas)
	    for stuff in stanzas:
	        # Break the tag storm stanza line apart into name-value tags by 
		# spliting on commas. Keep track of how many arrays are closed in this stanza, it will 
		# be needed later for updatind depth. The created nameValues list till carries the unecessary punctuation
		if not stuff: continue  # Throw out empty lines
		updateDepth = stuff.count("]")
	        splitByCommas = stuff.split(",")
	   	nameValues = list(splitByCommas)
		for nameValue in nameValues:
		    # Iterate over the various name value pairs in a given stanza
		    # Remove the unwanted puntuation, and increase the depth when 
		    # a name appears with no value (indicating a child stanza)
		    if not nameValue: continue # Throw out empty lines 
		    splitByColon = nameValue.split(":")
		    NVPair = list(splitByColon)
		    if not NVPair[1]: 
		        depth += 1
		        # The name value pairs with no value are not printed. 
			continue
	  	    prettyPrint (NVPair[0].translate(None, '"{}[],'),
		    		NVPair[1].translate(None,'{}[],"'), depth)
		# Update the depth to account for closed arrays 
		depth -= updateDepth
		print ()# print a new line at the end of each stanza

if __name__ == "__main__" :
    sys.exit(main(sys.argv))
