/* correlate - Correlate two columns in tab-separated file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "correlate.h"

static char const rcsid[] = "$Id: correlate.c,v 1.1 2010/05/27 05:59:46 kent Exp $";

int xCol = 0;
int yCol = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "correlate - Correlate two columns in tab-separated file.\n"
  "usage:\n"
  "   correlate input.tab\n"
  "options:\n"
  "   -xCol=N - which is first column to correlate, default 0\n"
  "   -yCol=N - which is second column to correlate, default 1\n"
  );
}

static struct optionSpec options[] = {
   {"xCol", OPTION_INT},
   {"yCol", OPTION_INT},
   {NULL, 0},
};

void correlate(char *file)
/* correlate - Correlate two columns in tab-separated file.. */
{
struct correlate *c = correlateNew(0);
int rowSize = max(xCol, yCol) + 1;
char *row[rowSize];
struct lineFile *lf = lineFileOpen(file, TRUE);
while (lineFileRow(lf, row))
    {
    double x = lineFileNeedDouble(lf, row, xCol);
    double y = lineFileNeedDouble(lf, row, yCol);
    correlateNext(c, x, y);
    }
printf("%g\n", correlateResult(c));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
xCol = optionInt("xCol", xCol);
yCol = optionInt("yCol", yCol);
correlate(argv[1]);
return 0;
}
