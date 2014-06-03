/* headRest - Return all *but* the first N lines of a file.. */

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
  "headRest - Return all *but* the first N lines of a file.\n"
  "usage:\n"
  "   headRest count fileName\n"
  "You can use stdin for fileName\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void headRest(char *countString, char *inName)
/* headRest - Return all *but* the first N lines of a file.. */
{
int count;
int i;
char *line;
struct lineFile *lf = lineFileOpen(inName, TRUE);
if (!isdigit(countString[0]))
    usage();
count = atoi(countString);
for (i=0; i<count; ++i)
     lineFileNext(lf, &line, NULL);
while (lineFileNext(lf, &line, NULL))
     puts(line);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
headRest(argv[1], argv[2]);
return 0;
}
