/* geneBand - Find bands for all genes. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "refLink.h"
#include "genePred.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geneBand - Find bands for all genes\n"
  "usage:\n"
  "   geneBand database outFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void printBands(struct refLink *rl, FILE *f)
/* Print name of genes and bands it occurs on. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct genePred *gp;
char query[512];
int count = 0;
struct dyString *bands = newDyString(0);
struct dyString *pos = newDyString(0);
char band[64];

sprintf(query, "select * from refGene where name = '%s'", rl->mrnaAcc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++count;
    gp = genePredLoad(row);
    if (hChromBand(gp->chrom, (gp->txStart + gp->txEnd)/2, band))
        dyStringPrintf(bands, "%s,", band);
    else
        dyStringPrintf(bands, "n/a,");
    dyStringPrintf(pos, "%s:%d-%d,", gp->chrom, gp->txStart+1, gp->txEnd);
    }
if (count > 0)
    fprintf(f, "%s\t%s\t%d\t%s\t%s\n", rl->name, rl->mrnaAcc, count, 
    	bands->string, pos->string);

dyStringFree(&bands);
dyStringFree(&pos);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void geneBand(char *database, char *outFile)
/* geneBand - Find bands for all genes. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct refLink rl;
FILE *f = mustOpen(outFile, "w");

fprintf(f, "#name\trefSeq\tbands\npositions\n");
sr = sqlGetResult(conn, "select * from refLink");
while ((row = sqlNextRow(sr)) != NULL)
    {
    refLinkStaticLoad(row, &rl);
    printBands(&rl, f);
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
hSetDb(argv[1]);
geneBand(argv[1], argv[2]);
return 0;
}
