/* addCols - Add together columns. */
#include "common.h"
#include "linefile.h"
#include "options.h"

int clMaxCols=16;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "addCols - Sum columns in a text file.\n"
  "usage:\n"
  "   addCols <fileName>\n"
  "adds all columns in the given file, \n"
  "outputs the sum of each column.  <fileName> can be the\n"
  "name: stdin to accept input from stdin.\n"
  "Options:\n"
  "    -maxCols=N - maximum number of colums (defaults to %d)\n", clMaxCols);
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"maxCols", OPTION_INT},
   {NULL, 0},
};


void addCols(char *fileName)
/* addCols - Add together columns. */
{
struct lineFile *lf;
char **words;
AllocArray(words, clMaxCols);
int wordCount;
double *totals;
AllocArray(totals, clMaxCols);
int maxCount = 0;
int i;

lf = lineFileOpen(fileName, TRUE);
while ((wordCount = lineFileChopNext(lf, words, clMaxCols)) != 0)
    {
    if (maxCount < wordCount)
        maxCount = wordCount;
    for (i=0; i<wordCount; ++i)
        totals[i] += atof(words[i]);
    }
for (i=0; i<maxCount; ++i)
     printf("%6.2f ", totals[i]);
printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clMaxCols = optionInt("maxCols", clMaxCols);
addCols(argv[1]);
return 0;
}
