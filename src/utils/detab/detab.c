/* detab - remove tabs from program. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


int tabSize = 8;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "detab - remove tabs from program\n"
  "usage:\n"
  "   detab inFile outFile\n"
  "options:\n"
  "   -tabSize=N (default 8)\n"
  "Note currently this just replaces tabs with tabSize\n"
  "spaces.  Tabs actually add a variable number of spaces\n"
  "if properly implemented,  going to the next column that's\n"
  "an even multiple of tabSize.   If you need this please\n"
  "implement it and put in a -proper flag or something. -jk\n"
  );
}

void detab(char *inFile, char *outFile)
/* detab - remove tabs from program. */
{
struct lineFile *lf = lineFileOpen(inFile, FALSE);
FILE *f = mustOpen(outFile, "w");
char *line;
int lineSize, i;

while (lineFileNext(lf, &line, &lineSize))
    {
    for (i=0; i<lineSize; ++i)
        {
	char c = line[i];
	if (c == '\t')
	    spaceOut(f, tabSize);
	else
	    fputc(c, f);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
tabSize = optionInt("tabSize", tabSize);
if (argc != 3)
    usage();
detab(argv[1], argv[2]);
return 0;
}
