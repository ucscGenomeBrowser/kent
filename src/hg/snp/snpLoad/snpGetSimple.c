/* snpGetSimple - get simple SNPs. */
/* create a hash of chrom names that contains output file handles */

#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpGetSimple.c,v 1.2 2006/08/18 00:01:07 heather Exp $";

static struct hash *chromHash = NULL;
static struct hash *annotationsHash = NULL;


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpGetSimple - get simple SNPs, divided by chrom\n"
    "usage:\n"
    "    snpGetSimple database snpTable annotationsTable\n");
}

void loadChroms()
/* hash all chromNames, create file handles */
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
    safef(fileName, sizeof(fileName), "snpGetSimple.%s", row[0]);
    f = mustOpen(fileName, "w");
    hashAdd(chromHash, cloneString(row[0]), f);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void loadMultiples(char *tableName)
/* create a hash of SNPs that align more than once, so we can exclude these */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
FILE *f;
struct hashEl *hel = NULL;
char *snpId = NULL;
char *annotation = NULL;

annotationsHash = newHash(0);
safef(query, sizeof(query), "select name, exception from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    annotation = cloneString(row[1]);
    if (!sameString(annotation, "MultipleAlignments")) continue;
    snpId = cloneString(row[0]);
    hel = hashLookup(annotationsHash, snpId);
    if (hel == NULL)
        hashAdd(annotationsHash, snpId, NULL);
    }
}

boolean triAllelic(char *observed)
{
if (sameString(observed, "A/C/G")) return TRUE;
if (sameString(observed, "A/C/T")) return TRUE;
if (sameString(observed, "A/G/T")) return TRUE;
if (sameString(observed, "C/G/T")) return TRUE;
return FALSE;
}

void getSnps(char *tableName)
/* spew out simple SNPs */
/* exclude SNPs on random chroms */
/* exclude SNPs that align multiple places */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashCookie cookie;
char *randomString = NULL;
struct hashEl *hel = NULL;

safef(query, sizeof(query), 
    "select name, chrom, chromStart, chromEnd, strand, refUCSC, observed, class, locType from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    randomString = strstr(row[1], "random");
    if (randomString != NULL) continue;
    if (!sameString(row[7], "single")) continue;
    if (!sameString(row[8], "exact")) continue;
    if (triAllelic(row[6])) continue;
    if (sameString(row[6], "A/C/G/T")) continue;
    hel = hashLookup(annotationsHash, row[0]);
    if (hel != NULL) continue;
    hel = hashLookup(chromHash, row[1]);
    if (hel == NULL)
        verbose(1, "%s not found\n", row[1]);
    else
        fprintf(hel->val, "%s %s %s %s 0 %s %s %s\n", row[1], row[2], row[3], row[0], row[4], row[5], row[6]);
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

loadChroms();
loadMultiples(argv[2]);
getSnps(snpTableName);

return 0;
}
