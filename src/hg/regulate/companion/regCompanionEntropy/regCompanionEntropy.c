/* regCompanionEntropy - Calculate entropy from tab-separated file with IDs in first col and expn 
 * values in other columns. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionEntropy - Calculate entropy from tab-separated file with IDs in first col and expn\n"
  "values in other columns. Output is two column: id<tab>entropy\n"
  "usage:\n"
  "   regCompanionEntropy input.tab output.tab\n"
  "options:\n"
  "   -ignoreAllZero\n"
  );
}

static struct optionSpec options[] = {
   {"ignoreAllZero", OPTION_BOOLEAN},
   {NULL, 0},
};

void regCompanionEntropy(char *input, char *output)
/* regCompanionEntropy - Calculate entropy from tab-separated file with IDs in first col and expn 
 * values in other columns. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *words[256];
double vals[256], x;
int wordCount;
while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    if (wordCount < 3)
       errAbort("Need at least two numbers in a line for this to work.");
    int i;
    char *id = words[0];
    double sum = 0;
    double maxX = 0;
    for (i=1; i<wordCount; ++i)
	{
	vals[i-1] = x = lineFileNeedDouble(lf, words, i);
	if (x > maxX)
	    maxX = x;
	if (x < 0)
	   errAbort("All numbers should be positive.  Got %g line %d of %s\n", 
	   	x, lf->lineIx, lf->fileName);
	sum += x;
	}

    /* Normalize so whole row of numbers adds to 1. */
    if (sum == 0)
	{
	if (optionExists("ignoreAllZero"))
	    continue;
	else
	    errAbort("Need at least one positive number in line %d of %s\n", 
	    	lf->lineIx, lf->fileName);
	}
    int valCount = wordCount - 1;
    for (i=0; i<valCount; ++i)
        vals[i] /= sum;

    double h = 0;	/* entropy */
    for (i=0; i<valCount; ++i)
	{
	x = vals[i];
	if (x > 0)
	    h += x*log(x);
	}
    if (h != 0)
	h /= -log(2);

    fprintf(f, "%s\t%g\t%g\n", id, maxX, h);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
regCompanionEntropy(argv[1], argv[2]);
return 0;
}
