/* genePredCheck - validate genePred files or tables. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
char *gChromSizes = NULL;
int gErrCount = 0;  /* global count of errors */
int gChkCount = 0;  /* global count of genes checked */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredCheck - validate genePred files or tables\n"
  "usage:\n"
  "   genePredCheck [options] fileTbl ..\n"
  "\n"
  "If fileTbl is an existing file, then it is checked.  Otherwise, if -db\n"
  "is provided, then a table by this name in db is checked.\n"
  "\n"
  "options:\n"
  "   -db=db - If specified, then this database is used to\n"
  "          get chromosome sizes, and perhaps the table to check.\n"
  "   -chromSizes=file.chrom.sizes - use chrom sizes from tab separated\n"
  "          file (name,size) instead of from chromInfo table in specified db.");
}

static void checkAGenePred(char *fileTbl, int iRec, struct genePred *gp,
                           struct hash* chromSizes)
/* check one genePred */
{
char desc[2*PATH_LEN];

safef(desc, sizeof(desc), "%s:%d", fileTbl, iRec);
if (chromSizes != NULL)
    gErrCount += genePredCheckChromSizes(desc, stderr, gp, chromSizes);
else
    gErrCount += genePredCheckDb(desc, stderr, gDb, gp);
gChkCount++;
}

static void checkGenePred(char *fileTbl)
/* check a genePred file or table */
{
struct sqlConnection *conn = NULL;
struct genePredReader *gpr = NULL;
struct genePred *gp;
struct hash* chromSizes =
    (gChromSizes != NULL) ? hChromSizeHashFromFile(gChromSizes) : NULL;
int iRec = 0;

if (fileExists(fileTbl))
    {
    gpr = genePredReaderFile(fileTbl, NULL);
    }
else if (gDb != NULL)
    {
    conn = hAllocConn(gDb);
    gpr = genePredReaderQuery(conn, fileTbl, NULL);
    }
else
    {
    errAbort("file %s doesn't exist, must specify -db=db if this is a table", fileTbl);
    }

while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    checkAGenePred(fileTbl, ++iRec, gp, chromSizes);
    genePredFree(&gp);
    }
genePredReaderFree(&gpr);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int iarg;
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
gDb = optionVal("db", NULL);
gChromSizes = optionVal("chromSizes", NULL);
for (iarg = 1; iarg < argc; iarg++)
    checkGenePred(argv[iarg]);
verbose(1, "checked: %d failed: %d\n", gChkCount, gErrCount);
return ((gErrCount == 0) ? 0 : 1);
}
