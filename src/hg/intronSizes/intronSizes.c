/* intronSizes - Output list of intron sizes.. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "genePred.h"

static char const rcsid[] = "$Id: intronSizes.c,v 1.1 2005/05/12 00:04:06 baertsch Exp $";

char *chromName;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "intronSizes - Output list of intron sizes.\n" 
  "usage:\n"
  "   intronSizes database geneTable\n"
  "options:\n"
  "   -chrom=chrN - restrict to a particular chromosome\n"
  "   -withUtr - restrict to genes with UTR regions\n"
  );
}

void intronSizes(char *database, char *table)
/* intronSizes - Output list of intron sizes.. */
{
struct dyString *query = newDyString(1024);
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct genePred *gp;
int exonIx, txStart;
int rowOffset;
char strand;

hSetDb(database);
rowOffset = hOffsetPastBin(NULL, table);
conn = hAllocConn(database);
dyStringPrintf(query, "select * from %s", table);
if (chromName != NULL)
    dyStringPrintf(query, " where chrom = '%s'", chromName);
if (cgiBoolean("withUtr"))
    {
    dyStringPrintf(query, " %s txStart != cdsStart", 
        (chromName == NULL ? "where" : "and"));
    }
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    strand = gp->strand[0];
    txStart = gp->txStart;
    for (exonIx=1; exonIx < gp->exonCount; ++exonIx)
        {
        int intronStart = gp->exonEnds[exonIx-1];
        int intronEnd = gp->exonStarts[exonIx];
        printf("%s\t%d\t%d\t%s\t%d\n",
                gp->chrom, intronStart, intronEnd, 
                gp->name, intronEnd - intronStart);
        }
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
chromName = cgiOptionalString("chrom");
if (argc != 3)
    usage();
intronSizes(argv[1], argv[2]);
return 0;
}
