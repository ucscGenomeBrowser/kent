/* intronSizes - Output list of intron sizes.. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "genePred.h"


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

void genePredIntrons(struct genePred *gp, struct bed **bedList)
/* get list of introns (bed format) for a genePred */
/* append list of introns to bedList */
{
int exonIx=1;
struct bed *bed = NULL;

for (exonIx=1; exonIx < gp->exonCount; ++exonIx)
    {
    int intronStart = gp->exonEnds[exonIx-1];
    int intronEnd = gp->exonStarts[exonIx];
    if (intronEnd > intronStart)
        {
        AllocVar(bed);
        bed->chrom = cloneString(gp->chrom);
        bed->chromStart = intronStart;
        bed->chromEnd = intronEnd;
        bed->name = cloneString(gp->name);
        bed->score = intronEnd-intronStart;
        bed->strand[0] = gp->strand[0];
        bed->strand[1] = gp->strand[1];
        slAddHead(bedList, bed);
        }
    }
}

void intronSizes(char *database, char *table)
/* intronSizes - Output list of intron sizes.. */
{
struct dyString *query = newDyString(1024);
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct genePred *gp;
int rowOffset;
struct bed *bedList = NULL, *bed = NULL;

hSetDb(database);
rowOffset = hOffsetPastBin(NULL, table);
conn = hAllocConn(database);
sqlDyStringPrintf(query, "select * from %s", table);
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
    genePredIntrons(gp, &bedList);
    slReverse(&bedList);
    for (bed = bedList ; bed != NULL ; bed=bed->next)
        bedTabOutN(bed,6, stdout);
    bedFreeList(&bedList);
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
