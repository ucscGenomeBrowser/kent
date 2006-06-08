/* illuminaLookup - utility program to get coords. */

#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: illuminaLookup.c,v 1.1 2006/06/08 18:23:01 heather Exp $";

struct snpSubset 
    {
    char *chrom;
    int start;
    int end;
    char strand;
    char *observed;
    char *class;
    char *locType;
    char *func;
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
    "illuminaLookup - get coords using rsId\n"
    "usage:\n"
    "    illuminaLookup db illuminaTable snpTable outputFile\n");
}


struct hash *storeSnps(char *tableName)
/* store subset data for SNPs in a hash */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct snpSubset *subsetElement = NULL;

verbose(1, "creating hash...\n");
ret = newHash(16);
safef(query, sizeof(query), 
      "select name, chrom, chromStart, chromEnd, strand, observed, class, locType, func from %s", 
      tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(subsetElement);
    subsetElement->chrom = cloneString(row[1]);
    subsetElement->start = sqlUnsigned(row[2]);
    subsetElement->end = sqlUnsigned(row[3]);
    subsetElement->strand = row[4][0];
    subsetElement->observed = cloneString(row[5]);
    subsetElement->class = cloneString(row[6]);
    subsetElement->locType = cloneString(row[7]);
    subsetElement->func = cloneString(row[8]);
    hashAdd(ret, cloneString(row[0]), subsetElement);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}


void processSnps(struct hash *snpHash, char *illuminaTable, char *fileName)
/* read illuminaTable */
/* lookup details in snpHash*/
/* report if SNP missing */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
struct snpSubset *subsetElement = NULL;
FILE *fileHandle = mustOpen(fileName, "w");

verbose(1, "process SNPs...\n");
safef(query, sizeof(query), "select dbSnpId, chrom, name from %s", illuminaTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(snpHash, row[0]);
    if (hel == NULL) 
        {
	fprintf(stderr, "%s not found\n", row[0]);
	continue;
	}
    subsetElement = (struct snpSubset *)hel->val;
    fprintf(fileHandle, "%s\t%s\t%d\t%d\t%c\t%s\t%s\t%s\t%s\n", 
        row[0],
        subsetElement->chrom,
        subsetElement->start,
        subsetElement->end,
	subsetElement->strand,
	subsetElement->observed,
	subsetElement->class,
	subsetElement->locType,
	subsetElement->func
	);
    }

carefulClose(&fileHandle);
}


int main(int argc, char *argv[])
/* load SNPs into hash */
{
char *snpDb = NULL;
char *illuminaTableName = NULL;
char *snpTableName = NULL;
char fileName[64];

struct hash *snpHash = NULL;

if (argc != 5)
    usage();

snpDb = argv[1];
illuminaTableName = argv[2];
snpTableName = argv[3];
safef(fileName, ArraySize(fileName), "%s", argv[4]);

/* process args */
hSetDb(snpDb);
if (!hTableExists(illuminaTableName))
    errAbort("no %s table in %s\n", illuminaTableName, snpDb);
if (!hTableExists(snpTableName))
    errAbort("no %s table in %s\n", snpTableName, snpDb);

snpHash = storeSnps(snpTableName);
processSnps(snpHash, illuminaTableName, fileName);

return 0;
}
