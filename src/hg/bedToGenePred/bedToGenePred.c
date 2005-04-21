/* bedToGenePred - convert bed format files to genePred format */
#include "common.h"
#include "options.h"
#include "bed.h"
#include "genePred.h"
#include "linefile.h"

static char const rcsid[] = "$Id: bedToGenePred.c,v 1.1 2005/04/21 03:39:28 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

/* command line options */

void usage(msg)
/* Explain usage and exit. */
{
errAbort("%s:\n"
    "bedToGenePred - convert bed format files to genePred format\n"
    "usage:\n"
    "   bedToGenePred bedFile genePredFile\n"
    "\n"
    "Convert a bed file to a genePred file. BED should have at least 12 columns.\n", msg);
}

void cnvBedToGenePred(char *bedFile, char *genePredFile)
/* convert bed format files to genePred format */
{
struct lineFile *bedLf = lineFileOpen(bedFile, TRUE);
FILE *gpFh = mustOpen(genePredFile, "w");
char *row[12];


while (lineFileNextRow(bedLf, row, ArraySize(row)))
    {
    struct bed *bed = bedLoad12(row);
    struct genePred* gp = bedToGenePred(bed);
    genePredTabOut(gp, gpFh);
    genePredFree(&gp);
    bedFree(&bed);
    }

carefulClose(&gpFh);
lineFileClose(&bedLf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
cnvBedToGenePred(argv[1], argv[2]);
return 0;
}

