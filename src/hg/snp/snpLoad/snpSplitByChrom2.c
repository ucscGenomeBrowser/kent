/* snpSplitByChrom2 - part of ortho pipeline. */

#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpSplitByChrom2.c,v 1.1 2006/08/17 03:20:22 heather Exp $";

static struct hash *chromHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpSplitByChrom2 - part of ortho pipeline\n"
    "usage:\n"
    "    snpSplitByChrom2 database table outputFileName\n");
}

void loadChroms(char *outputFileName)
/* hash chromNames, create file handles */
/* skip random and hap chroms */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
FILE *f;
char fileName[64];
char *randomString = NULL;
char *hapString = NULL;

chromHash = newHash(0);
safef(query, sizeof(query), "select chrom from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    randomString = strstr(row[0], "random");
    if (randomString != NULL) continue;
    hapString = strstr(row[0], "hap");
    if (hapString != NULL) continue;
    safef(fileName, sizeof(fileName), "%s.%s", outputFileName, row[0]);
    f = mustOpen(fileName, "w");
    hashAdd(chromHash, cloneString(row[0]), f);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void getSnps(char *tableName)
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashCookie cookie;
char *randomString = NULL;
struct hashEl *hel = NULL;

safef(query, sizeof(query), 
    "select chrom, chromStart, chromEnd, name, strand, humanAllele, observed from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(chromHash, row[0]);
    if (hel == NULL)
        verbose(1, "%s not found\n", row[0]);
    else
        fprintf(hel->val, "%s %s %s %s 0 %s %s %s\n", row[0], row[1], row[2], row[3], row[4], row[5], row[6]);
    }
sqlFreeResult(&sr);
cookie = hashFirst(chromHash);
while (hel = hashNext(&cookie))
    fclose(hel->val);
}


int main(int argc, char *argv[])
{

char *snpDb = NULL;
char *snpTableName = NULL;

if (argc != 4)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

snpTableName = argv[2];

/* check that tables exist */
if (!hTableExists(snpTableName))
    errAbort("no %s table in %s\n", snpTableName, snpDb);
if (!hTableExists("chromInfo"))
    errAbort("no chromInfo table in %s\n", snpDb);

loadChroms(argv[3]);
getSnps(snpTableName);

return 0;
}
