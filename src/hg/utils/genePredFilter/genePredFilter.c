/* genePredFilter - filter a genePred file. */

/* Copyright (C) 2015 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "options.h"
#include "verbose.h"
#include "portable.h"
#include "hdb.h"
#include "genePred.h"
#include "genePredReader.h"


/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"chromSizes", OPTION_STRING},
    {NULL, 0}
};
char *gDb = NULL;
int gKeepCount = 0;
int gDropCount = 0;
static char *sizesFile = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredFilter - filter a genePred file\n"
  "usage:\n"
  "   genePredFilter [options] genePredIn genePredOut\n"
  "\n"
  "Filter a genePredFile, dropping invalid entries\n"
  "\n"
  "options:\n"
  "   -db=db - If specified, then this database is used to\n"
  "         get chromosome sizes.\n"
  "   -chromSizes=file.chrom.sizes - use chrom sizes from tab separated\n"
  "         file (name,size) instead of from chromInfo table in specified db.\n"
  "   -verbose=2 - level >= 2 prints out errors for each problem found.");
}

static boolean filterAGenePred(char *fileTbl, int iRec, struct genePred *gp, FILE* errFh, struct hash *chromSizes)
/* check one genePred, True if it should be kept */
{
char desc[2*PATH_LEN];
safef(desc, sizeof(desc), "%s:%d", fileTbl, iRec);
if (chromSizes != NULL)
    return genePredCheckChromSizes(desc, errFh, gp, chromSizes) == 0;
else
    return genePredCheckDb(desc, errFh, gDb, gp) == 0;
}

static void genePredFilter(char *genePredIn, char *genePredOut)
/* filter a genePred file */
{
struct genePredReader *gpr = genePredReaderFile(genePredIn, NULL);
FILE *outFh = mustOpen(genePredOut, "w");
FILE *errFh = (verboseLevel() < 2) ? mustOpen("/dev/null", "w") : stderr;
struct genePred *gp;
int iRec = 0;
struct hash *chromSizes =
   (sizesFile != NULL) ? hChromSizeHashFromFile(sizesFile) : NULL;

while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    if (filterAGenePred(genePredIn, ++iRec, gp, errFh, chromSizes))
        {
        gKeepCount++;
        genePredTabOut(gp, outFh);
        }
    else
        gDropCount++;
    }
carefulClose(&errFh);
genePredReaderFree(&gpr);
verbose(1, "kept: %d dropped: %d\n", gKeepCount, gDropCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
gDb = optionVal("db", NULL);
sizesFile = optionVal("chromSizes", NULL);
genePredFilter(argv[1], argv[2]);
return 0;
}
