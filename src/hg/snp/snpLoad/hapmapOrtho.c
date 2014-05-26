/* hapmapOrtho */
/* get hapmapOrtho table from overall snpOrtho table */
/* hash in contents of hapmap table */
/* serially read snpOrtho table */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
    "hapmapOrtho - get hapmapAllelesOrtho table from overall snpOrtho table\n"
    "usage:\n"
    "    hapmapOrtho db hapmapTable snpOrthoTable\n");
}

struct coords
    {
    char *chrom;
    int start;
    int end;
    };

struct hash *storeHapmap(char *tableName)
/* store coords */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct coords *coordItem = NULL;

ret = newHash(16);
sqlSafef(query, sizeof(query), "select name, chrom, chromStart, chromEnd from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(coordItem);
    coordItem->chrom = cloneString(row[1]);
    coordItem->start = sqlUnsigned(row[2]);
    coordItem->end = sqlUnsigned(row[3]);
    hashAdd(ret, cloneString(row[0]), coordItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}


void processSnps(char *snpTableName, struct hash *hapmapHash)
/* read snpTable, lookup in hapmapHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
char *rsId = NULL;
int start = 0;
int end = 0;
FILE *outputFileHandle = mustOpen("hapmapOrtho.tab", "w");
FILE *errorFileHandle = mustOpen("hapmapOrtho.err", "w");
struct coords *coordItem = NULL;

sqlSafef(query, sizeof(query), 
    "select chrom, chromStart, chromEnd, name, orthoScore, strand, refUCSC, observed, "
    "orthoChrom, orthoStart, orthoEnd, orthoStrand, orthoAllele from %s", 
    snpTableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rsId = cloneString(row[3]);
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    /* all hapmap data is single base */
    /* don't include lift if it wasn't also single base */
    if (end != start + 1) 
        {
        fprintf(errorFileHandle, "skipping %s due to size %d\n", rsId, end-start);
	continue;
	}
    hel = hashLookup(hapmapHash, rsId);
    if (hel == NULL) continue;
    coordItem = (struct coords *)hel->val;
    if (differentString(coordItem->chrom, row[0])) continue;
    if (coordItem->start != start) continue;
    if (coordItem->end != end) continue;
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t", row[0], row[1], row[2], rsId);
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t", row[4], row[5], row[6], row[7]);
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t%s\n", row[8], row[9], row[10], row[11], row[12]);
    }

carefulClose(&outputFileHandle);
carefulClose(&errorFileHandle);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* get hapmapOrtho table from overall snpOrtho table */
/* hash in contents of hapmap table */
{
char *database = NULL;
char *hapmapTableName = NULL;
char *snpOrthoTableName = NULL;
struct hash *hapmapHash = NULL;

if (argc != 4)
    usage();

database = argv[1];
hapmapTableName = argv[2];
snpOrthoTableName = argv[3];

hSetDb(database);
if (!hTableExists(hapmapTableName))
    errAbort("no %s table in %s\n", hapmapTableName, database);
if (!hTableExists(snpOrthoTableName))
    errAbort("no %s table in %s\n", snpOrthoTableName, database);

hapmapHash = storeHapmap(hapmapTableName);
processSnps(snpOrthoTableName, hapmapHash);

return 0;
}
