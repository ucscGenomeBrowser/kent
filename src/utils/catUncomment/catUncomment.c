/* catUncomment - Concatenate input removing lines that start with '#'. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "catUncomment - Concatenate input removing lines that start with '#'\n"
  "Output goes to stdout\n"
  "usage:\n"
  "   catUncomment file(s)\n");
}

void catUncomment(int inCount, char *inNames[])
/* catUncomment - Concatenate input removing lines that start with '#'. */
{
struct lineFile *lf;
char *fileName;
char *line;
int i, lineSize;

for (i=0; i<inCount; ++i)
    {
    fileName =  inNames[i];
    lf = lineFileOpen(fileName, FALSE);
    while (lineFileNext(lf, &line, &lineSize))
        {
	if (line[0] != '#')
	    mustWrite(stdout, line, lineSize);
	}
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
catUncomment(argc-1, argv+1);
return 0;
}
