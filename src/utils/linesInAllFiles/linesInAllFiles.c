/* linesInAllFiles - Print lines that are in all input files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "linesInAllFiles - Print lines that are in all input files to stdout.\n"
  "usage:\n"
  "   linesInAllFiles in1.txt in2.txt ... inN.txt\n"
  "The order of output will follow the order in the last file.\n"
  "This only puts out the first occurence of the line if it occurs multiple times\n"
  "options:\n"
  "   -col=N - if set, file is tab-delimited and just the given column (starting with 1)\n"
  "            In this case only that column will be output\n"
  );
}

int col = -1;

static struct optionSpec options[] = {
   {"col", OPTION_INT,},
   {NULL, 0},
};

char *next(struct lineFile *lf)
/* Return next input from file or NULL if at end. */
{
if (col <= 0)
    {
    char *line;
    if (!lineFileNext(lf, &line, NULL))
       return NULL;
    return line;
    }
else
    {
    char *row[col];
    if (!lineFileRow(lf, row))
        return NULL;
    return row[col-1];
    }
}

void linesInAllFiles(int inCount, char *inFiles[])
/* linesInAllFiles - Print lines that are in all input files.. */
{
/* Hash first file. */
struct hash *curHash = hashNew(17);
struct lineFile *lf = lineFileOpen(inFiles[0], TRUE);
char *s;
while ((s = next(lf)) != NULL)
    hashStore(curHash, s);
lineFileClose(&lf);

/* For middle files just replace hash with a smaller one. */
int i;
int lastIn = inCount-1;
for (i=1; i<lastIn; ++i)
    {
    struct hash *nextHash = hashNew(17);
    lf = lineFileOpen(inFiles[i], TRUE);
    while ((s = next(lf)) != NULL)
        {
	if (hashLookup(curHash, s) != NULL)
	    hashStore(nextHash, s);
	}
    lineFileClose(&lf);
    hashFree(&curHash);
    curHash = nextHash;
    }

/* For last one print out hits. */
struct hash *uniqHash = hashNew(17);
lf = lineFileOpen(inFiles[lastIn], TRUE);
while ((s = next(lf)) != NULL)
    {
    if (hashLookup(curHash, s) != NULL)
	{
	if (hashLookup(uniqHash, s) == NULL)
	    printf("%s\n", s);
	else
	    hashAdd(uniqHash,s, NULL);
	}
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
col = optionInt("col", col);
if (col > 1000)
   errAbort("col must be a number between 1 and 1000");
linesInAllFiles(argc-1, argv+1);
return 0;
}
