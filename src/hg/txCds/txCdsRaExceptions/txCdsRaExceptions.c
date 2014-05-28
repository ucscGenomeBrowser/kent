/* txCdsRaExceptions - Mine exceptional things like selenocysteine out of ra file.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsRaExceptions - Mine exceptional things like selenocysteine out of genbank ra file.\n"
  "usage:\n"
  "   txCdsRaExceptions in.ra output\n"
  "The output is two column: accession exception.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void txCdsRaExceptions(char *inRa, char *outFile)
/* txCdsRaExceptions - Mine exceptional things like selenocysteine out of ra file.. */
{
struct lineFile *lf = lineFileOpen(inRa, TRUE);
FILE *f = mustOpen(outFile, "w");
char acc[256];
acc[0] = 0;
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *tag = nextWord(&line);
    if (tag != NULL)
	{
	if (sameString(tag, "acc"))
	    {
	    char *val = trimSpaces(line);
	    safef(acc, sizeof(acc), "%s", val);
	    }
	if (sameString(tag, "selenocysteine") || sameString(tag, "translExcept")
	    || sameString(tag, "exception"))
	    {
	    char *val = trimSpaces(line);
	    fprintf(f, "%s\t%s\t%s\n", acc, tag, val);
	    }
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
txCdsRaExceptions(argv[1], argv[2]);
return 0;
}
