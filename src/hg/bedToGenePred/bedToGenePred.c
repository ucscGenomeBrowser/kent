/* bedToGenePred - convert bed format files to genePred format */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "bed.h"
#include "genePred.h"
#include "linefile.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

/* command line options */

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s:\n"
    "bedToGenePred - convert bed format files to genePred format\n"
    "usage:\n"
    "   bedToGenePred bedFile genePredFile\n"
    "\n"
    "Convert a bed file to a genePred file. If BED has at least 12 columns,\n"
    "then a genePred with blocks is created. Otherwise single-exon genePreds are\n"
    "created.\n", msg);
}


/* convert one line read from a bed file to a genePred */
void cnvBedRec(char *line, FILE *gpFh)
{
char *row[12];
int numCols = chopByWhite(line, row, ArraySize(row));
if (numCols < 4)
    errAbort("bed must have at least 4 columns");
struct bed *bed = bedLoadN(row, numCols);
struct genePred* gp = bedToGenePred(bed);
genePredTabOut(gp, gpFh);
genePredFree(&gp);
bedFree(&bed);
}

void cnvBedToGenePred(char *bedFile, char *genePredFile)
/* convert bed format files to genePred format */
{
struct lineFile *bedLf = lineFileOpen(bedFile, TRUE);
FILE *gpFh = mustOpen(genePredFile, "w");
char *line;

while (lineFileNextReal(bedLf, &line))
    cnvBedRec(line, gpFh);

carefulClose(&gpFh);
lineFileClose(&bedLf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage("Too few arguments");
cnvBedToGenePred(argv[1], argv[2]);
return 0;
}

