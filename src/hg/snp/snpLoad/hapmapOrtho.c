/* hapmapOrtho */
/* get hapmapOrtho table from overall snpOrtho table */
/* hash in contents of hapmap table */
/* serially read snpOrtho table */

#include "common.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hapmapOrtho.c,v 1.1 2007/02/22 01:04:29 heather Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
    "hapmapOrtho - get hapmapAllelesOrtho table from overall snpOrtho table\n"
    "usage:\n"
    "    hapmapOrtho db hapmapTable snpOrthoTable\n");
}


struct hash *storeHapmap(char *tableName)
/* store names only */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

ret = newHash(16);
safef(query, sizeof(query), "select name from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(ret, cloneString(row[0]), NULL);
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
FILE *outputFileHandle = mustOpen("hapmapOrtho.tab", "w");

safef(query, sizeof(query), 
    "select chrom, chromStart, chromEnd, name, orthoScore, strand, refUCSC, observed, "
    "orthoChrom, orthoStart, orthoEnd, orthoStrand, orthoAllele from %s", 
    snpTableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rsId = cloneString(row[3]);
    hel = hashLookup(hapmapHash, rsId);
    if (hel == NULL) continue;
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t", row[0], row[1], row[2], rsId);
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t", row[4], row[5], row[6], row[7]);
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t%s\n", row[8], row[9], row[10], row[11], row[12]);
    }

carefulClose(&outputFileHandle);
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
