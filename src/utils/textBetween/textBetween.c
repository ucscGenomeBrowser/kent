/* textBetween - Print all text in file between two input strings.. */

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
  "textBetween - Print all text in file between two input strings including the strings.\n"
  "usage:\n"
  "   textBetween before after fileName\n"
  "options:\n"
  "   -withEnds - If set include the before and after strings in the output\n"
  );
}

static struct optionSpec options[] = {
   {"withEnds", OPTION_BOOLEAN},
   {NULL, 0},
};

void textBetween(char *before, char *after, char *fileName)
/* textBetween - Print all text in file between two input strings.. */
{
char *gulp;
size_t gulpSize;
readInGulp(fileName, &gulp, &gulpSize);
char *s = stringBetween(before, after, gulp);
if (s == NULL)
    errAbort("Couldn't find anything");
if (withEnds)
    fputs(before, stdout);
fputs(s, stdout);
if (withEnds)
    fputs(after, stdout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
withEnds = optionExists("withEnds");
if (argc != 4)
    usage();
textBetween(argv[1], argv[2], argv[3]);
return 0;
}
