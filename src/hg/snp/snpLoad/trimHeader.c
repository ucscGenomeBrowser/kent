/* trimHeader */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "trimHeader - Util for bosTau3.\n"
  "usage:\n"
  "  trimHeader inputfile \n");
}


void doTrimHeader(char *inputFileName)
{
FILE *outputFileHandle = mustOpen("trimHeader.out", "w");
struct lineFile *lf = lineFileOpen(inputFileName, TRUE);
char *line;
int lineSize;
char *row[5], *contigId[2];

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] != '>')
        {
	fprintf(outputFileHandle, "%s\n", line);
	continue;
	}
    chopString(line, ".", contigId, ArraySize(row));
    fprintf(outputFileHandle, "%s\n", contigId[0]);
    }

carefulClose(&outputFileHandle);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
{
char *inputFileName = NULL;

if (argc != 2)
    usage();

inputFileName = argv[1];

doTrimHeader(inputFileName);

return 0;
}
