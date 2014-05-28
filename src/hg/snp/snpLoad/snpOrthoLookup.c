/* snpOrthoLookup - integrate data from base assembly. */
/* liftOver drops chrom, chromStart, chromEnd. */
/* liftOver does retain allele and variant. */
/* read snpSimpleTable -- this contains the input set */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


static char *database = NULL;
static char *snpTable = NULL;
static char *orthoTable = NULL;
struct hash *snpHash = NULL;
FILE *outputFileHandle = NULL;

struct snpSubset 
    {
    char *chrom;
    int start;
    int end;
    char *strand;
    char *refUCSC;
    char *observed;
    };


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpOrthoLookup - integrate data from base assembly\n"
    "usage:\n"
    "    snpOrthoLookup database snpSimpleTable orthoTable\n");
}


struct hash *getBaseSnpData(char *tableName)
/* put all snpSubset elements in snpHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct snpSubset *subsetElement = NULL;
struct hash *ret = newHash(16);
int count = 0;

verbose(1, "get base snp data\n");
sqlSafef(query, sizeof(query), 
    "select chrom, chromStart, chromEnd, name, strand, refUCSC, observed from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(subsetElement);
    subsetElement->chrom = cloneString(row[0]);
    subsetElement->start = sqlUnsigned(row[1]);
    subsetElement->end = sqlUnsigned(row[2]);
    subsetElement->strand = cloneString(row[4]);
    subsetElement->refUCSC = cloneString(row[5]);
    subsetElement->observed = cloneString(row[6]);
    hashAdd(ret, cloneString(row[3]), subsetElement);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "%d rows in hash\n", count);
return ret;
}

void writeResults(char *tableName)
/* read through preliminary table with chimp and macaque alleles. */
/* lookup into snpHash. */
{
struct hashEl *hel= NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct snpSubset *subsetElement = NULL;
int bin = 0;
int start = 0;
int end = 0;

verbose(1, "write results...\n");
sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, species, strand, allele from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(snpHash, row[3]);
    if (hel == NULL) continue;
    subsetElement = (struct snpSubset *)hel->val;
    start = subsetElement->start;
    end = subsetElement->end;
    bin = hFindBin(start, end);
    fprintf(outputFileHandle, "%d\t%s\t%d\t%d\t%s\t", bin, subsetElement->chrom, start, end, row[3]); 
    fprintf(outputFileHandle, "0\t%s\t%s\t%s\t", subsetElement->strand, subsetElement->refUCSC, subsetElement->observed);
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t%s\t%s\n", row[4], row[0], row[1], row[2], row[5], row[6]);
    }

}


int main(int argc, char *argv[])
{
if (argc != 4)
    usage();

database = argv[1];
hSetDb(database);
snpTable = argv[2];
if (!hTableExistsDb(database, snpTable))
    errAbort("can't get table %s\n", snpTable);
orthoTable = argv[3];
if (!hTableExistsDb(database, orthoTable))
    errAbort("can't get table %s\n", orthoTable);

snpHash = getBaseSnpData(snpTable);
outputFileHandle = mustOpen("snpOrthoLookup.tab", "w");
writeResults(orthoTable);
carefulClose(&outputFileHandle);

return 0;
}
