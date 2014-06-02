/* replaceTextBetween - Replaces a section of text in the middle of a file.. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

/* Command line variables. */
boolean withEnds;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "replaceTextBetween - Replaces a section of text in the middle of a file.\n"
  "usage:\n"
  "   replaceTextBetween before after mainFile insertFile\n"
  "where before and after are strings that bracket the text to replace in mainFile.\n"
  "options:\n"
  "   -withEnds - If set include the before and after strings in the replaced portion\n"
  );
}

static struct optionSpec options[] = {
   {"withEnds", OPTION_BOOLEAN},
   {NULL, 0},
};

void replaceTextBetween(char *start, char *end, char *outerFile, char *middleFile)
/* replaceTextBetween - Replaces a section of text in the middle of a file.. */
{
/* Read outer file into memory. */
char *outer;
size_t outerSize;
readInGulp(outerFile, &outer, &outerSize);

/* Figure out the boundaries of the region we want to replace. */
char *s = stringIn(start, outer);
if (s == NULL)
    errAbort("Can't find '%s' in %s", start, outerFile);
char *e = stringIn(end, s);
if (e == NULL)
    errAbort("Can't find '%s' in %s", end, outerFile);
if (withEnds)
    {
    e += strlen(end);
    }
else
    {
    s += strlen(start);
    }


/* Read middle file into memory. */
char *middle;
size_t middleSize;
readInGulp(middleFile, &middle, &middleSize);

/* Write out file in three parts. */
int startSize = s - outer;
mustWrite(stdout, outer, startSize);
mustWrite(stdout, middle, middleSize);
int endSize = outer + outerSize - e;
mustWrite(stdout, e, endSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
withEnds = optionExists("withEnds");
if (argc != 5)
    usage();
replaceTextBetween(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
