/* colTransform - Add and/or multiply column by constant.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "colTransform - Add and/or multiply column by constant.\n"
  "usage:\n"
  "   colTransform column input.tab addFactor mulFactor output.tab\n"
  "where:\n"
  "   column is the column to transform, starting with 1\n"
  "   input.tab is the tab delimited input file\n"
  "   addFactor is what to add.  Use 0 here to not change anything\n"
  "   mulFactor is what to multiply by.  Use 1 here not to change anything\n"
  "   output.tab is the tab delimited output file\n"
  );
}

void colTransform(char *column, char *input, char *addFactor, char *mulFactor, char *output)
/* colTransform - Add and/or multiply column by constant.. */
{
int col = sqlUnsigned(column) - 1;
double add = sqlDouble(addFactor);
double mul = sqlDouble(mulFactor);
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *words[512];
int wordCount;
while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    lineFileExpectAtLeast(lf, col, wordCount);
    double x = lineFileNeedDouble(lf, words, col);
    int i;
    for (i=0; i<wordCount; ++i)
        {
	if (i != 0)
	    fputc('\t', f);
	if (i == col)
	    fprintf(f, "%g", x*mul+add);
	else
	    fputs(words[i], f);
	}
    fputc('\n', f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 6)
    usage();
colTransform(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
