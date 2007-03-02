/* subColumn - Substitute one column in a tab-separated file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: subColumn.c,v 1.1 2007/03/02 00:44:26 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "subColumn - Substitute one column in a tab-separated file.\n"
  "usage:\n"
  "   subColumn column in.tab sub.tab out.tab\n"
  "Where:\n"
  "    column is the column number (starting with 1)\n"
  "    in.tab is a tab-separated file\n"
  "    sub.tab is a where first column is old values, second new\n"
  "    out.tab is the substituted output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void subColumn(char *asciiColumn, char *inFile, char *subFile, char *outFile)
/* subColumn - Substitute one column in a tab-separated file.. */
{
struct hash *subHash = hashTwoColumnFile(subFile);
int column = atoi(asciiColumn);
if (column == 0)
    usage();
else
    column -= 1;
char *row[1024*4];
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
int rowCount;
while ((rowCount = lineFileChopNextTab(lf, row, ArraySize(row))) > 0)
    {
    if (rowCount == ArraySize(row))
        errAbort("Too many columns (%d) line %d of  %s.", rowCount, lf->lineIx, lf->fileName);
    if (column >= rowCount)
        errAbort("Not enough columns (%d) line %d of  %s.", rowCount, lf->lineIx, lf->fileName);
    int i;
    for (i=0; i<rowCount; ++i)
	{
	char *s = row[i];
	if (i == column)
	    {
	    char *sub = hashFindVal(subHash, s);
	    if (sub == NULL)
	        errAbort("%s not in %s line %d of %s", s, subFile, lf->lineIx, lf->fileName);
	    s = sub;
	    }
	fputs(s, f);
	if (i == rowCount-1)
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
if (argc != 5)
    usage();
subColumn(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
