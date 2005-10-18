/* minEachRow - Output numeric value for each row. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: minEachRow.c,v 1.1 2005/10/18 05:48:34 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "minEachRow - Output numeric value for each row\n"
  "usage:\n"
  "   minEachRow input output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void minEachRow(char *inName, char *outName)
/* minEachRow - Output numeric value for each row. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *line;
if (lineFileNextReal(lf, &line))
    {
    int rowSize = chopByWhite(line, NULL, 0);
    char **row;
    AllocArray(row, rowSize+1);
    for (;;)
        {
	int thisSize = chopByWhite(line, row, rowSize+1);
	int i;
	double minVal;
	if (thisSize != rowSize)
	    errAbort("First line has %d words, but line %d has %d words",
	    	rowSize, lf->lineIx, thisSize);
	minVal = atof(row[0]);
	for (i=1; i<rowSize; ++i)
	    {
	    double val = atof(row[i]);
	    if (val < minVal)
	        minVal = val;
	    }
	fprintf(f, "%f\n", minVal);
	if (!lineFileNextReal(lf, &line))
	    break;
	}
    carefulClose(&f);
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
minEachRow(argv[1], argv[2]);
return 0;
}
