/* genePredStats - get stats on genes in a genePred file. */
#include "common.h"
#include "options.h"
#include "genePred.h"
#include "genePredReader.h"

static char const rcsid[] = "$Id: genePredStats.c,v 1.1 2005/12/02 05:51:55 markd Exp $";

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};
char *clDb = NULL;

/* type for function used to get stats */
typedef void (*statsFuncType)(struct genePred *gp, FILE *outFh);

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n\n"
  "genePredStats - get stats on genes in a genePred file\n"
  "usage:\n"
  "   genePredStats [options] what genePredFile statsOut\n"
  "\n"
  "options:\n"
  "\n"
  "The what arguments indicates the type of output.  Many types of output\n"
  "are designed for making histograms. The following values are current\n"
  "implemented:\n"
  "   exonSizeHisto - sizes of exons, ones per line\n",
  msg);
}

void exonSizeHisto(struct genePred *gp, FILE *outFh)
/* stats functions to get date for histogram of exon sizes */
{
int i;
for (i = 0; i < gp->exonCount; i++)
    fprintf(outFh, "%d\n", (gp->exonEnds[i]-gp->exonStarts[i]));
}

statsFuncType getStatsFunc(char *what)
/* look up the stats function to use */
{
if (sameString(what, "exonSizeHisto"))
    return exonSizeHisto;
else
    usage("invalid what argument");
return NULL;
}

void genePredStats(char *what, char *gpFile, char *outFile)
/* get stats on genes in a genePred file */
{
struct genePredReader *gpr = genePredReaderFile(gpFile, NULL);
statsFuncType statsFunc = getStatsFunc(what);
struct genePred *gp;
FILE *outFh = mustOpen(outFile, "w");

while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    statsFunc(gp, outFh);
    genePredFree(&gp);
    }
carefulClose(&outFh);
genePredReaderFree(&gpr);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong number of arguments");
genePredStats(argv[1], argv[2], argv[3]);
return 0;
}
