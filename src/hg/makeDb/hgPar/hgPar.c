/* hgPar - create PAR track. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "psl.h"
#include "parSpec.h"
#include "hdb.h"
#include "hgRelate.h"
#include "jksql.h"

static char const rcsid[] = "$Id: hgPar.c,v 1.1 2010/02/17 08:54:21 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgPar - create PAR track\n"
  "usage:\n"
  "   hgPar db parSpecFile parTable\n"
  "options:\n"
  "  -fileOutput - parTable is a file to write instead of a table\n"
  "Format of parSpecFile are rows of the following tab-separated values:\n"
  "   name chromA startA endA chromB startB endB\n"
  "\n"
  "Size of regions must be the same.  Order of chromosomes is not important/\n"
  "Name is something like 'par1', 'par2'.  The resulting table is in PSL\n"
  "format with a record for each chromosome\n"
  );
}

static struct optionSpec options[] = {
    {"fileOutput", OPTION_BOOLEAN},
    {NULL, 0},
};

static struct psl *mkParPsl(char *qName, unsigned qSize, int qStart, int qEnd,
                            char *tName, unsigned tSize, int tStart, int tEnd)
/* create one of the PAR psls */
{
if ((qEnd - qStart) != (tEnd - tStart))
    errAbort("size of PAR locations must be the same: %s:%d-%d and %s:%d-%d",
             qName, qStart, qEnd, tName, tStart, tEnd);
struct psl* psl = pslNew(qName, qSize, qStart, qEnd, tName, tSize, tStart, tEnd,
                         "+", 1, 0);
psl->match = qEnd - qStart;
psl->blockCount = 1;
psl->blockSizes[0] = qEnd - qStart;
psl->qStarts[0] = qStart;
psl->tStarts[0] = tStart;
return psl;
}

static struct psl *convertParSpec(struct sqlConnection *conn, struct parSpec *parSpec)
/* convert a parSpec object two PSLs. */
{
int chromASize = hChromSize(sqlGetDatabase(conn), parSpec->chromA);
int chromBSize = hChromSize(sqlGetDatabase(conn), parSpec->chromB);

return slCat(mkParPsl(parSpec->chromA, chromASize, parSpec->startA, parSpec->endA,
                      parSpec->chromB, chromBSize, parSpec->startB, parSpec->endB),
             mkParPsl(parSpec->chromB, chromBSize, parSpec->startB, parSpec->endB,
                      parSpec->chromA, chromASize, parSpec->startA, parSpec->endA));
}

static void loadTable(struct psl *psls, struct sqlConnection *conn, char *parTable)
/* create and load table */
{
char *sqlCmd = pslGetCreateSql(parTable, PSL_TNAMEIX,  hGetMinIndexLength(sqlGetDatabase(conn)));
sqlRemakeTable(conn, parTable, sqlCmd);
FILE *tabFh = hgCreateTabFile(NULL, parTable);
struct psl *psl;
for (psl = psls; psl != NULL; psl = psl->next)
    pslTabOut(psl, tabFh);
hgLoadTabFile(conn, NULL, parTable, &tabFh);
hgUnlinkTabFile(NULL, parTable);
}

static void hgPar(char *db, char *parSpecFile, char *parTable, boolean fileOutput)
/* hgPar - create PAR track. */
{
struct sqlConnection *conn = sqlConnect(db);
struct parSpec *parSpec, *parSpecs = parSpecLoadAll(parSpecFile);
struct psl *psls = NULL;

for (parSpec = parSpecs; parSpec != NULL; parSpec = parSpec->next)
    psls = slCat(psls, convertParSpec(conn, parSpec));

slSort(&psls, pslCmpTarget);
if (fileOutput)
    pslWriteAll(psls, parTable, FALSE);
else
    loadTable(psls, conn, parTable);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgPar(argv[1], argv[2], argv[3], optionExists("fileOutput"));
return 0;
}
