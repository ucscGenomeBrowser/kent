/* snpCompareWeight - compare weight in old and new snp tables. */
/* Generate counts */
/* Store weight as a string */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


struct weightList
    {
    struct weightList *next;
    char *name;
    char *weight;
    };

FILE *countFileHandle;
FILE *logFileHandle;

int oneToOne;
int oneToTwo;
int oneToThree;
int twoToTwo;
int twoToOne;
int twoToThree;
int threeToThree;
int threeToOne;
int threeToTwo;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCompareWeight - compare old and new snp tables\n"
    "must be in the same database\n"
    "usage:\n"
    "    snpCompareWeight snpDb oldTable newTable\n");
}

boolean addIfNew(struct hash *myhash, char *name)
{
struct hashEl *hel = NULL;

hel = hashLookup(myhash, name);
if (hel == NULL)
    {
    hashAdd(myhash, cloneString(name), NULL);
    return TRUE;
    }
return FALSE;
}

struct hash *getDuplicateNameHash(char *tableName)
/* return hash with names that occur more than once */
/* use a hash with all names to figure it out */
{
struct hash *nameHash = NULL;
struct hash *duplicateNameHash = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

nameHash = newHash(16);
duplicateNameHash = newHash(16);

verbose(1, "getDuplicateNameHash for %s...\n", tableName);

sqlSafef(query, sizeof(query), "select name from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!addIfNew(nameHash, row[0]))
        addIfNew(duplicateNameHash, row[0]);
    }
sqlFreeResult(&sr);
return duplicateNameHash;
}


struct weightList *getTableList(char *tableName)
/* store weight for singly aligning SNPs in a list */
/* first store non-unique names */
{
struct weightList *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *helName = NULL;
struct weightList *el = NULL;
int count = 0;
struct hash *duplicateNameHash = getDuplicateNameHash(tableName);

sqlSafef(query, sizeof(query), "select name, weight from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    helName = hashLookup(duplicateNameHash, row[0]);
    if (helName != NULL) continue;
    count++;
    AllocVar(el);
    el->name = cloneString(row[0]);
    el->weight = cloneString(row[1]);
    slAddHead(&ret, el);
    }
sqlFreeResult(&sr);
verbose(1, "%d singly-aligning names in %s\n", count, tableName);
hFreeConn(&conn);
return ret;

}

struct hash *getTableHash(char *tableName)
/* store weight for singly aligning SNPs in a hash */
/* first store non-unique names */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *helName = NULL;
int count = 0;
struct hash *duplicateNameHash = getDuplicateNameHash(tableName);

ret = newHash(16);
sqlSafef(query, sizeof(query), "select name, weight from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    helName = hashLookup(duplicateNameHash, row[0]);
    if (helName != NULL) continue;
    count++;
    hashAdd(ret, cloneString(row[0]), cloneString(row[1]));
    }
sqlFreeResult(&sr);
verbose(1, "%d singly-aligning names in %s\n", count, tableName);
hFreeConn(&conn);
return ret;
}

void updateCounters(char *name, int oldWeight, int newWeight)
{

if (oldWeight == newWeight)
    {
    if (oldWeight == 1) oneToOne++;
    if (oldWeight == 2) twoToTwo++;
    if (oldWeight == 3) threeToThree++;
    return;
    }

fprintf(logFileHandle, "%s: old weight %d, new weight %d\n", name, oldWeight, newWeight);
if (oldWeight == 1 && newWeight == 2) oneToTwo++;
if (oldWeight == 1 && newWeight == 3) oneToThree++;
if (oldWeight == 2 && newWeight == 1) twoToOne++;
if (oldWeight == 2 && newWeight == 3) twoToThree++;
if (oldWeight == 3 && newWeight == 1) threeToOne++;
if (oldWeight == 3 && newWeight == 2) threeToTwo++;

}



void processSnps(struct weightList *oldTableList, struct hash *newTableHash)
/* loop through oldTableHash */
/* compare to newTableHash */
/* if SNP missing from newTableHash, write to logFile*/
{
struct weightList *listPtr = NULL;
struct hashEl *helNew = NULL;
int oldWeight = 0;
int newWeight = 0;

verbose(1, "process SNPs...\n");
for (listPtr = oldTableList; listPtr != NULL; listPtr = listPtr->next)
    {
    helNew = hashLookup(newTableHash, listPtr->name);
    if (helNew == NULL) 
        {
	fprintf(logFileHandle, "%s (old weight %s) not found in new\n", listPtr->name, listPtr->weight);
	continue;
	}
    oldWeight = atoi(listPtr->weight);
    newWeight = atoi(helNew->val);
    updateCounters(listPtr->name, oldWeight, newWeight);
    }
}


int main(int argc, char *argv[])
/* work with singly aligning SNPs only */
/* load oldTable subset into list */
/* load newTable subset into hash */
{

char *snpDb = NULL;
struct weightList *oldTableList = NULL;
struct hash *newTableHash = NULL;
char *oldTableName = NULL;
char *newTableName = NULL;

if (argc != 4)
    usage();

oneToOne = 0;
oneToTwo = 0;
oneToThree = 0;
twoToTwo = 0;
twoToOne = 0;
twoToThree = 0;
threeToThree = 0;
threeToOne = 0;
threeToTwo = 0;

snpDb = argv[1];
hSetDb(snpDb);

oldTableName = argv[2];
newTableName = argv[3];

// check that tables exist
if (!hTableExists(oldTableName))
    errAbort("no %s table in %s\n", oldTableName, snpDb);
if (!hTableExists(newTableName))
    errAbort("no %s table in %s\n", newTableName, snpDb);

oldTableList = getTableList(oldTableName);
newTableHash = getTableHash(newTableName);

logFileHandle = mustOpen("snpCompareWeightLog.out", "w");
processSnps(oldTableList, newTableHash);
carefulClose(&logFileHandle);

countFileHandle = mustOpen("snpCompareWeightCounts.out", "w");
fprintf(countFileHandle, "oneToOne = %d\n", oneToOne);
fprintf(countFileHandle, "oneToTwo = %d\n", oneToTwo);
fprintf(countFileHandle, "oneToThree = %d\n", oneToThree);
fprintf(countFileHandle, "twoToTwo = %d\n", twoToTwo);
fprintf(countFileHandle, "twoToOne = %d\n", twoToOne);
fprintf(countFileHandle, "twoToThree = %d\n", twoToThree);
fprintf(countFileHandle, "threeToThree = %d\n", threeToThree);
fprintf(countFileHandle, "threeToOne = %d\n", threeToOne);
fprintf(countFileHandle, "threeToTwo = %d\n", threeToTwo);
carefulClose(&countFileHandle);

return 0;
}
