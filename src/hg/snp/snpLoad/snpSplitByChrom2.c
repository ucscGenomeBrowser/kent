/* snpSplitByChrom2 - part of ortho pipeline. */

#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpSplitByChrom2.c,v 1.3 2006/08/17 21:11:05 heather Exp $";

static struct hash *chromHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpSplitByChrom2 - part of ortho pipeline\n"
    "usage:\n"
    "    snpSplitByChrom2 database table\n");
}

void loadChroms()
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
char *chromName1 = NULL;
char *chromName2 = NULL;

chromHash = newHash(0);
// okay to use 3 args here?
safef(query, sizeof(query), "select chrom from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromName1 = cloneString(row[0]);
    randomString = strstr(chromName1, "random");
    if (randomString != NULL) continue;
    chromName2 = cloneString(row[0]);
    hapString = strstr(chromName2, "hap");
    if (hapString != NULL) continue;
    safef(fileName, sizeof(fileName), "%s_snp126hg18ortho.tab", row[0]);
    f = mustOpen(fileName, "w");
    verbose(1, "chrom = %s\n", row[0]);
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
    "select chrom, chromStart, chromEnd, name, strand, humanAllele, humanObserved from %s", 
    tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(chromHash, row[0]);
    if (hel == NULL)
        verbose(1, "%s not found\n", row[0]);
    else
        fprintf(hel->val, "%s\t%s\t%s\t%s\t0\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3], row[4], row[5], row[6]);
    }
sqlFreeResult(&sr);
cookie = hashFirst(chromHash);
while (hel = hashNext(&cookie))
    fclose(hel->val);
}

void createTable(char *chromName)
/* create a chrN_snp126hg18ortho table */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    chrom varchar(15) not null default '',\n"
"    chromStart int(10) not null default '0',\n"
"    chromEnd int(10) not null default '0',\n"
"    name varchar(15) not null default '',\n"
"    score smallint(5) not null default '0',\n"
"    strand enum('?','+','-') not null default '?',\n"
"    humanAllele enum('A','C','G','T'),\n"
"    humanObserved varchar(255)\n"
");\n";

struct dyString *dy = newDyString(1024);

safef(tableName, ArraySize(tableName), "%s_snp126hg18ortho", chromName);
dyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
hFreeConn(&conn);
}

void loadDatabase(char *chromName)
/* load one table into database */
{
FILE *f;
struct sqlConnection *conn = hAllocConn();
char tableName[64], fileName[64];

safef(tableName, ArraySize(tableName), "%s_snp126hg18ortho", chromName);
safef(fileName, ArraySize(fileName), "%s_snp126hg18ortho.tab", chromName);

f = mustOpen(fileName, "r");
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
{

char *snpDb = NULL;
char *snpTableName = NULL;
struct hashCookie cookie;
char *chromName = NULL;

if (argc != 3)
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
getSnps(snpTableName);

verbose(1, "creating tables...\n");
cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    createTable(chromName);

verbose(1, "loading database...\n");
cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    verbose(1, "chrom = %s\n", chromName);
    loadDatabase(chromName);
    }

return 0;
}
