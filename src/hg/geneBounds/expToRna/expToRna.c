/* expToRna - Make a little two column table that associates 
 * rnaClusters with expression info. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "bed.h"
#include "binRange.h"

static char const rcsid[] = "$Id: expToRna.c,v 1.3.338.1 2008/07/31 02:24:00 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "expToRna - Make a little two column table that associates \n"
  "rnaClusters with expression info\n"
  "usage:\n"
  "   expToRna database rnaTable expTable output.tab\n"
  "options:\n"
  "   -chrom=chr22 - restrict to a chromosome (default is whole genome)\n"
  );
}

int dupeCount = 0;
int uniqCount = 0;
int missCount = 0;
int hitCount = 0;

void doOneChrom(char *database, char *chrom, char *rnaTable, char *expTable, FILE *f)
/* Process one chromosome. */
{
int chromSize = hChromSize(database, chrom);
struct binKeeper *bk = binKeeperNew(0, chromSize);
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct bed *exp, *rna;
int rowOffset;
struct binElement *be, *beList;
int oneCount;

/* Load up expTable into bin-keeper. */
sr = hChromQuery(conn, expTable, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    exp = bedLoadN(row + rowOffset, 12);
    binKeeperAdd(bk, exp->chromStart, exp->chromEnd, exp);
    }
sqlFreeResult(&sr);

/* Loop through rnaTable and look at intersections. */
sr = hChromQuery(conn, rnaTable, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rna = bedLoadN(row + rowOffset, 12);
    beList = binKeeperFind(bk, rna->chromStart, rna->chromEnd);
    oneCount = 0;
    for (be = beList; be != NULL; be = be->next)
        {
	exp = be->val;
	if (exp->strand[0] == rna->strand[0])
	    {
	    ++oneCount;
	    ++hitCount;
//	    fprintf(f, "%s:%d-%d\t%s\t%s\n", 
//	    	rna->chrom, rna->chromStart, rna->chromEnd, rna->name, exp->name);
	    }
	}
    slFreeList(&beList);
    if (oneCount == 0)
	{
        ++missCount;
	fprintf(f, "miss %s:%d-%d %c %s\n", rna->chrom, rna->chromStart, rna->chromEnd, rna->strand[0], rna->name);
	}
    else if (oneCount == 1)
	{
	fprintf(f, "uniq %s:%d-%d %c %s\n", rna->chrom, rna->chromStart, rna->chromEnd, rna->strand[0], rna->name);
        ++uniqCount;
	}
    else
	{
	fprintf(f, "dupe %s:%d-%d %c %s\n", rna->chrom, rna->chromStart, rna->chromEnd, rna->strand[0], rna->name);
        ++dupeCount;
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void expToRna(char *database, char *rnaTable, char *expTable, char *outName)
/* expToRna - Make a little two column table that associates 
 * rnaClusters with expression info. */
{
struct slName *chromList = NULL, *chromEl;
char *chrom = optionVal("chrom", NULL);
FILE *f = mustOpen(outName, "w");

if (chrom != NULL)
    chromList = newSlName(chrom);
else
    chromList = hAllChromNamesDb(database);
for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    chrom = chromEl->name;
    uglyf("%s\n", chrom);
    doOneChrom(database, chrom, rnaTable, expTable, f);
    }
printf("%d dupe, %d uniq, %d miss, %d total, %d hits\n", 
	dupeCount, uniqCount, missCount,
	dupeCount + uniqCount + missCount,
	hitCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
expToRna(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
