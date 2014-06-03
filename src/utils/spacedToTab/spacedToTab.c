/* spacedToTab - Convert fixed width space separated fields to tab separated. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "spacedColumn.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "spacedToTab - Convert fixed width space separated fields to tab separated\n"
  "Note this requires two passes, so it can't be done on a pipe\n"
  "usage:\n"
  "   spacedToTab in.txt out.tab\n"
  "options:\n"
  "   -sizes=X,Y,Z - Force it to have columns of the given widths.\n"
  "                 The final char in each column should be space or newline\n"
  );
}

static struct optionSpec options[] = {
   {"sizes", OPTION_STRING},
   {NULL, 0},
};

void spacedToTab(char *inFile, char *outFile, char *colSizes)
/* spacedToTab - Convert fixed width space separated fields to tab separated. */
{
struct spacedColumn *colList;
if (colSizes != NULL)
    colList = spacedColumnFromSizeCommaList(colSizes);
else
    colList = spacedColumnFromFile(inFile);
int colCount = slCount(colList);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *line;
int lineSize;
FILE *f = mustOpen(outFile, "w");
char *row[colCount];

while (lineFileNext(lf, &line, &lineSize))
    {
    if (!spacedColumnParseLine(colList, line, row))
         errAbort("Short line %d of %s\n", lf->lineIx, lf->fileName);
    int i, lastCol = colCount-1;
    for (i = 0; i<=lastCol; ++i)
        {
	fputs(row[i], f);
	if (i == lastCol)
	    fputc('\n', f);
	else
	    fputc('\t', f);
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
char *colSizes = optionVal("sizes", NULL);
spacedToTab(argv[1],argv[2], colSizes);
return 0;
}
