/* Test program to see if synmap is reading correctly */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "synMap.h"


void usage()
{
errAbort("synMapTest - tries to read in a synMap file like those produced\n"
	 "by Sanja and Robert for the mouse-human synteny map.\n"
	 "usage:\n"
	 "\tsynMapTest <file>\n");
}

int main(int argc, char *argv[])
{
struct synMap *smList = NULL;
if(argc != 2)
    usage();
warn("Loading from file %s.", argv[1]);
smList = synMapLoadAll(argv[1]);
warn("Loaded %d synMaps.", slCount(smList));
synMapFreeList(&smList);
warn("Done.");
return 0;
}
