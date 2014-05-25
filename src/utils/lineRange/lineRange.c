/* lineRange - Get a range of lines from file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "lineRange - Get a range of lines from file\n"
  "usage:\n"
  "   lineRange fileName start count\n"
  "Print <count> lines from fileName starting at 1-based <start>."
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void lineRange(char *fileName, int start, int count)
/* lineRange - Get a range of lines from file. */
{
struct lineFile *lf = lineFileOpen(fileName, FALSE);
char *line;
int lineSize;
int i;
if (count == 0 || start == 0)
    errAbort("Expecting positive number for start, count in command line");

/* Skip over first lines. */
for (i=1; i<start; ++i)
    {
    if (!lineFileNext(lf, &line, &lineSize))
        errAbort("%s doesn't have %d lines", fileName, start);
    }
/* Print coutn lines. */
for (i=0; i<count; ++i)
    {
    if (!lineFileNext(lf, &line, &lineSize))
         break;
    mustWrite(stdout, line, lineSize);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
lineRange(argv[1], atoi(argv[2]), atoi(argv[3]));
return 0;
}
