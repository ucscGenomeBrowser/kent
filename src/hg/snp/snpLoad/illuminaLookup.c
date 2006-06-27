/* illuminaLookup - utility program to get coords. */

#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: illuminaLookup.c,v 1.3 2006/06/08 21:18:47 heather Exp $";

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
    "    illuminaLookup db illuminaTable snpTable exceptionTable outputFile errorFile\n");
}


struct hash *storeExceptions(char *tableName)
/* store multiply-aligning SNPs */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

verbose(1, "reading exceptions...\n");
ret = newHash(0);
safef(query, sizeof(query), "select name, exception from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!sameString(row[1], "MultipleAlignments")) continue;
    hashAdd(ret, cloneString(row[0]), NULL);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

struct hash *storeSnps(char *tableName, struct hash *exceptionHash, char *errorFileName)
/* store subset data for SNPs in a hash */
/* exclude SNPs that align multiple places */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct snpSubset *subsetElement = NULL;
struct hashEl *hel = NULL;
FILE *errors = mustOpen(errorFileName, "w");

verbose(1, "creating SNP hash...\n");
ret = newHash(16);
safef(query, sizeof(query), 
      "select name, chrom, chromStart, chromEnd, strand, observed, class, locType, func from %s", 
      tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(exceptionHash, row[0]);
    if (hel != NULL)
        {
	fprintf(errors, "skipping %s, aligns more than one place\n", row[0]);
	continue;
	}
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
carefulClose(&errors);
return ret;
}






void processSnps(struct hash *snpHash, char *illuminaTable, char *fileName, char *errorFileName)
/* read illuminaTable */
/* lookup details in snpHash */
/* report if SNP missing */
/* report if class != single */
/* report if locType != exact */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
struct snpSubset *subsetElement = NULL;
FILE *fileHandle = mustOpen(fileName, "w");
FILE *errors = mustOpen(errorFileName, "w");
int bin = 0;

verbose(1, "process SNPs...\n");
safef(query, sizeof(query), "select dbSnpId, chrom, name from %s", illuminaTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(snpHash, row[0]);
    if (hel == NULL) 
        {
	fprintf(errors, "%s not found\n", row[0]);
	continue;
	}
    subsetElement = (struct snpSubset *)hel->val;

    if (!sameString(subsetElement->chrom, row[1]))
        {
	fprintf(errors, "unexpected chrom %s for snp %s\n", row[1], row[0]);
	continue;
	}

    if (!sameString(subsetElement->class, "single"))
	fprintf(errors, "unexpected class %s for snp %s\n", subsetElement->class, row[0]);
    if (!sameString(subsetElement->locType, "exact"))
	fprintf(errors, "unexpected locType %s for snp %s\n", subsetElement->locType, row[0]);

    bin = hFindBin(subsetElement->start, subsetElement->end);
    fprintf(fileHandle, "%d\t%s\t%d\t%d\t%s\t%s\n", 
        bin, subsetElement->chrom, subsetElement->start, subsetElement->end, row[0], row[2]);
    }

carefulClose(&fileHandle);
carefulClose(&errors);
}


int main(int argc, char *argv[])
/* load SNPs into hash */
{
char *snpDb = NULL;
char *illuminaTableName = NULL;
char *snpTableName = NULL;
char *exceptionTableName = NULL;
char fileName[64];
char errorFileName[64];

struct hash *snpHash = NULL;
struct hash *exceptionHash = NULL;

if (argc != 7)
    usage();

snpDb = argv[1];
illuminaTableName = argv[2];
snpTableName = argv[3];
exceptionTableName = argv[4];
safef(fileName, ArraySize(fileName), "%s", argv[5]);
safef(errorFileName, ArraySize(errorFileName), "%s", argv[6]);

/* process args */
hSetDb(snpDb);
if (!hTableExists(illuminaTableName))
    errAbort("no %s table in %s\n", illuminaTableName, snpDb);
if (!hTableExists(snpTableName))
    errAbort("no %s table in %s\n", snpTableName, snpDb);
if (!hTableExists(exceptionTableName))
    errAbort("no %s table in %s\n", exceptionTableName, snpDb);

exceptionHash = storeExceptions(exceptionTableName);
snpHash = storeSnps(snpTableName, exceptionHash, errorFileName);
processSnps(snpHash, illuminaTableName, fileName, errorFileName);

return 0;
}
