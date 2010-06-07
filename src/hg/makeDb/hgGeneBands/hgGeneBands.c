/* hgGeneBands - Find bands for all genes. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "refLink.h"
#include "genePred.h"

static char const rcsid[] = "$Id: hgGeneBands.c,v 1.4 2008/09/03 19:19:41 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGeneBands - Find bands for all genes\n"
  "usage:\n"
  "   hgGeneBands database outFile.tab\n"
  );
}

void printBands(char *database, struct refLink *rl, FILE *f)
/* Print name of genes and bands it occurs on. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct genePred *gp;
char query[512];
int count = 0;
struct dyString *bands = newDyString(0);
char band[64];

sprintf(query, "select * from refGene where name = '%s'", rl->mrnaAcc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++count;
    gp = genePredLoad(row);
    if (hChromBand(database, gp->chrom, (gp->txStart + gp->txEnd)/2, band))
        dyStringPrintf(bands, "%s,", band);
    else
        dyStringPrintf(bands, "n/a,");
    }
if (count > 0)
    fprintf(f, "%s\t%s\t%d\t%s\n", rl->name, rl->mrnaAcc, count, bands->string);

dyStringFree(&bands);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void hgGeneBands(char *database, char *outFile)
/* hgGeneBands - Find bands for all genes. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct refLink rl;
FILE *f = mustOpen(outFile, "w");

sr = sqlGetResult(conn, "select * from refLink");
while ((row = sqlNextRow(sr)) != NULL)
    {
    refLinkStaticLoad(row, &rl);
    printBands(database, &rl, f);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
hgGeneBands(argv[1], argv[2]);
return 0;
}
