/* hgAvidShortBed - Convert short form of AVID alignments to BED. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgAvidShortBed - Convert short form of AVID alignments to BED\n"
  "usage:\n"
  "   hgAvidShortBed input avidRepeat.bed avidUnique.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgAvidShortBed(char *inName, char *outRep, char *outUni)
/* hgAvidShortBed - Convert short form of AVID alignments to BED. */
{
struct lineFile *lf = lineFileOpen(inName, FALSE);
FILE *rep = mustOpen(outRep, "w");
FILE *uni = mustOpen(outUni, "w");
FILE *f = uni;
char *line;
int lineSize;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (startsWith("browser", line))
        ;	/* ignore. */
    else if (startsWith("track", line))
	{
	if (stringIn("non-repeat", line))
	    f = uni;
	else
	    f = rep;
	}
    else
        {
        mustWrite(f, line, lineSize);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
hgAvidShortBed(argv[1], argv[2], argv[3]);
return 0;
}
