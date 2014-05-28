/* snpMissing - compare old snp table to new snp table.
 * Check that missing SNPs are not in ContigLoc or MapInfo. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* Could use ContigLocFilter instead of ContigLoc. */
/* Could check that weight = 10 in MapInfo. */

#include "common.h"

#include "hash.h"
#include "hdb.h"



void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpMissing - compare old snp table to new snp table\n"
    "must be in the same database\n"
    "usage:\n"
    "    snpMissing snpDb oldTable newTable\n");
}

struct hash *getUniqueStringHash(char *columnName, char *tableName)
/* Read tableName.  Store column as string, unique values only. */
/* Remove 'rs'. */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *helName = NULL;
int count = 0;

ret = newHash(18);
sqlSafef(query, sizeof(query), "select %s from %s", columnName, tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    stripString(row[0], "rs");
    helName = hashLookup(ret, row[0]);
    if (helName == NULL)
        {
        hashAdd(ret, cloneString(row[0]), NULL);
	count++;
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "%d unique %s in %s\n", count, columnName, tableName);
return ret;
}


void processSnps(char *oldTableName, char *newTableName)
{
static struct hash *contigLocHash = NULL;
static struct hash *mapInfoHash = NULL;
static struct hash *oldNameHash = NULL;
static struct hash *newNameHash = NULL;

struct hashCookie cookie;
struct hashEl *helNameNew = NULL;
struct hashEl *contigLocElement = NULL;
struct hashEl *mapInfoElement = NULL;
char *name;
FILE *outputFileHandle = mustOpen("snpMissing.out", "w");

int count = 0;

verbose(1, "creating hashes...\n");
contigLocHash = getUniqueStringHash("snp_id", "ContigLoc");
mapInfoHash = getUniqueStringHash("snp_id", "MapInfo");
oldNameHash = getUniqueStringHash("name", oldTableName);
newNameHash = getUniqueStringHash("name", newTableName);

verbose(1, "writing results...\n");
cookie = hashFirst(oldNameHash);
while ((name = hashNextName(&cookie)) != NULL)
    {
    count++;
    helNameNew = hashLookup(newNameHash, name);
    if (helNameNew == NULL)
        {
	fprintf(outputFileHandle, "rs%s in %s but not in %s\n", name, oldTableName, newTableName);
        contigLocElement = hashLookup(contigLocHash, name);
	if (contigLocElement != NULL)
	    fprintf(outputFileHandle, "found in ContigLoc\n");
        mapInfoElement = hashLookup(mapInfoHash, name);
	if (mapInfoElement != NULL)
	    fprintf(outputFileHandle, "found in MapInfo\n");
	}

    if (count == 100000) break;
    }
carefulClose(&outputFileHandle);
}


int main(int argc, char *argv[])
{

char *snpDb = NULL;
char *oldTableName = NULL;
char *newTableName = NULL;

if (argc != 4)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

oldTableName = argv[2];
newTableName = argv[3];

/* check that tables exist */
if (!hTableExists(oldTableName))
    errAbort("no %s table in %s\n", oldTableName, snpDb);
if (!hTableExists(newTableName))
    errAbort("no %s table in %s\n", newTableName, snpDb);
if (!hTableExists("ContigLoc"))
    errAbort("no ContigLoc table in %s\n", snpDb);
if (!hTableExists("MapInfo"))
    errAbort("no MapInfo table in %s\n", snpDb);

processSnps(oldTableName, newTableName);

return 0;
}
