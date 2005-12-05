/* genePredHisto - get data for generating histograms from a genePred file. */
#include "common.h"
#include "options.h"
#include "genePred.h"
#include "genePredReader.h"

static char const rcsid[] = "$Id: genePredHisto.c,v 1.1 2005/12/05 22:20:49 markd Exp $";

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"ids", OPTION_BOOLEAN},
    {NULL, 0}
};
static boolean clIds = FALSE;  /* print ids */

/* type for function used to get histogram data */
typedef void (*histoFuncType)(struct genePred *gp, FILE *outFh);

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n\n"
         "genePredHisto - get data for generating histograms from a genePred file.\n"
         "usage:\n"
         "   genePredHisto [options] what genePredFile histoOut\n"
         "\n"
         "Options:\n"
         "  -ids - a second column with the gene name, useful for finding outliers.\n"
         "\n"
         "The what arguments indicates the type of output. The output file is\n"
         "a list of numbers suitable for input to textHistogram or similar\n"
         "The following values are current implemented\n"
         "   exonLen- length of exons\n",
         msg);
}


static void writeInt(FILE *outFh, struct genePred *gp, int val)
/* write a row with an integer value */
{
fprintf(outFh, "%d", val);
if (clIds)
    fprintf(outFh, "\t%s", gp->name);
fputc('\n', outFh);
}

static void exonLenHisto(struct genePred *gp, FILE *outFh)
/* function to get date for histogram of exon lengths */
{
int i;
for (i = 0; i < gp->exonCount; i++)
    writeInt(outFh, gp, (gp->exonEnds[i]-gp->exonStarts[i]));
}

static histoFuncType getHistoFunc(char *what)
/* look up the stats function to use */
{
if (sameString(what, "exonLen"))
    return exonLenHisto;
else
    usage("invalid what argument");
return NULL;
}

static void genePredHisto(char *what, char *gpFile, char *outFile)
/* get data for generating histograms from a genePred file. */
{
struct genePredReader *gpr = genePredReaderFile(gpFile, NULL);
histoFuncType histoFunc = getHistoFunc(what);
struct genePred *gp;
FILE *outFh = mustOpen(outFile, "w");

while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    histoFunc(gp, outFh);
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
clIds = optionExists("ids");
genePredHisto(argv[1], argv[2], argv[3]);
return 0;
}
