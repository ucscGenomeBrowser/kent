/* genePredCheck - validate genePred files or tables. */
#include "common.h"
#include "options.h"
#include "portable.h"
#include "hdb.h"
#include "genePred.h"
#include "genePredReader.h"

static char const rcsid[] = "$Id: genePredCheck.c,v 1.2 2005/12/02 04:56:10 markd Exp $";

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {NULL, 0}
};
char *gDb = NULL;
int gErrCount = 0;  /* global count of errors */

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
  "   -db=db - If specified, then this is this database is used to\n"
  "    get chromosome sizes, and perhaps the table to check.\n"
  "\n");
}

void checkGenePred(char *fileTbl)
/* check a genePred */
{
struct sqlConnection *conn = NULL;
struct genePredReader *gpr;
struct genePred *gp;
int chromSize = -1;  /* default to not checking */
char desc[512];
int iRec = 0;


if (gDb != NULL)
    {
    hSetDb(gDb);  /* needed for hChromSize */
    conn = hAllocConn(gDb);
    }

if (fileExists(fileTbl))
    {
    gpr = genePredReaderFile(fileTbl, NULL);
    }
else if (gDb != NULL)
    {
    gpr = genePredReaderQuery(conn, fileTbl, NULL);
    }
else
    {
    errAbort("file %s doesn't exist, must specify -db=db if this is a table", fileTbl);
    }

while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    if (gDb != NULL)
        chromSize = hChromSize(gp->chrom);
    iRec++;
    safef(desc, sizeof(desc), "%s:%d", fileTbl, iRec);
    gErrCount += genePredCheck(desc, stderr, chromSize, gp);
    }
genePredReaderFree(&gpr);
if (conn != NULL)
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
return ((gErrCount == 0) ? 0 : 1);
}
