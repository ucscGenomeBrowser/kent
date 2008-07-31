/* genePredCheck - validate genePred files or tables. */
#include "common.h"
#include "options.h"
#include "verbose.h"
#include "portable.h"
#include "hdb.h"
#include "genePred.h"
#include "genePredReader.h"
#include "chromInfo.h"

static char const rcsid[] = "$Id: genePredCheck.c,v 1.7.10.1 2008/07/31 02:24:01 markd Exp $";

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {NULL, 0}
};
char *gDb = NULL;
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
  "If fileTbl is an existing file, then it is check.  Otherwise, if -db\n"
  "is provided, then a table by this name is checked.\n"
  "\n"
  "options:\n"
  "   -db=db - If specified, then this database is used to\n"
  "    get chromosome sizes, and perhaps the table to check.\n"
  "\n");
}

static void checkAGenePred(char *fileTbl, int iRec, struct genePred *gp)
/* check one genePred */
{
int chromSize = -1;  /* default to not checking */
char desc[512];

safef(desc, sizeof(desc), "%s:%d", fileTbl, iRec);
if (gDb != NULL)
    {
    struct chromInfo *ci = hGetChromInfo(gDb, gp->chrom);
    if (ci == NULL)
        {
        fprintf(stderr, "Error: %s: %s has invalid chrom for %s: %s\n",
                desc, gp->name, gDb, gp->chrom);
        gErrCount++;
        chromSize = -1;  // don't validate
        }
    else
        chromSize = ci->size;
    }
gErrCount += genePredCheck(desc, stderr, chromSize, gp);
gChkCount++;
}

static void checkGenePred(char *fileTbl)
/* check a genePred file or table */
{
struct sqlConnection *conn = NULL;
struct genePredReader *gpr;
struct genePred *gp;
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
    checkAGenePred(fileTbl, ++iRec, gp);
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
for (iarg = 1; iarg < argc; iarg++)
    checkGenePred(argv[iarg]);
verbose(1, "checked: %d failed: %d\n", gChkCount, gErrCount);
return ((gErrCount == 0) ? 0 : 1);
}
