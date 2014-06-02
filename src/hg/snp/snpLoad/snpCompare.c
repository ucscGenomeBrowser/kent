/* snpCompare - compare old and new snp tables. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


struct snpSubsetList
    {
    struct snpSubsetList *next;
    char *name;
    char *chrom;
    int start;
    int end;
    char strand;
    char *observed;
    char *class;
    char *locType;
    // char *function;
    };

struct snpSubset 
    {
    char *chrom;
    int start;
    int end;
    char strand;
    char *observed;
    char *class;
    char *locType;
    // char *function;
    };


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCompare - compare old and new snp tables\n"
    "must be in the same database\n"
    "usage:\n"
    "    snpCompare snpDb oldTable newTable\n");
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


struct snpSubsetList *getTableList(char *tableName)
/* store subset data for singly aligning SNPs in a list */
/* first store non-unique names */
{
struct snpSubsetList *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *helName = NULL;
struct snpSubsetList *list = NULL;
struct snpSubsetList *el = NULL;
int count = 0;
struct hash *duplicateNameHash = getDuplicateNameHash(tableName);

sqlSafef(query, sizeof(query), 
      "select name, chrom, chromStart, chromEnd, strand, observed, class, locType from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    helName = hashLookup(duplicateNameHash, row[0]);
    if (helName != NULL) continue;
    count++;
    AllocVar(el);
    el->name = cloneString(row[0]);
    el->chrom = cloneString(row[1]);
    el->start = sqlUnsigned(row[2]);
    el->end = sqlUnsigned(row[3]);
    el->strand = row[4][0];
    el->observed = cloneString(row[5]);
    el->class = cloneString(row[6]);
    el->locType = cloneString(row[7]);
    slAddHead(&ret, el);
    }
sqlFreeResult(&sr);
verbose(1, "%d singly-aligning names in %s\n", count, tableName);
hFreeConn(&conn);
return ret;

}

struct hash *getTableHash(char *tableName)
/* store subset data for singly aligning SNPs in a hash */
/* first store non-unique names */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *helName = NULL;
struct snpSubset *subsetElement = NULL;
int count = 0;
struct hash *duplicateNameHash = getDuplicateNameHash(tableName);

ret = newHash(16);
sqlSafef(query, sizeof(query), 
      "select name, chrom, chromStart, chromEnd, strand, observed, class, locType from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    helName = hashLookup(duplicateNameHash, row[0]);
    if (helName != NULL) continue;
    count++;
    AllocVar(subsetElement);
    subsetElement->chrom = cloneString(row[1]);
    subsetElement->start = sqlUnsigned(row[2]);
    subsetElement->end = sqlUnsigned(row[3]);
    subsetElement->strand = row[4][0];
    subsetElement->observed = cloneString(row[5]);
    subsetElement->class = cloneString(row[6]);
    subsetElement->locType = cloneString(row[7]);
    hashAdd(ret, cloneString(row[0]), subsetElement);
    }
sqlFreeResult(&sr);
verbose(1, "%d singly-aligning names in %s\n", count, tableName);
hFreeConn(&conn);
return ret;
}

void compareAndLog(FILE *fileHandle, struct snpSubsetList *oldElement, struct snpSubset *newElement)
{
if (!sameString(oldElement->chrom, newElement->chrom))
    {
    fprintf(fileHandle, "coord mismatch (different chroms) %s\t", oldElement->name);
    fprintf(fileHandle, "old coords: %s:%d-%d\t", oldElement->chrom, oldElement->start, oldElement->end);
    fprintf(fileHandle, "new coords: %s:%d-%d\n", newElement->chrom, newElement->start, newElement->end);
    return;
    }

if (oldElement->start != newElement->start || oldElement->end != newElement->end)
    {
    fprintf(fileHandle, "coord mismatch %s\t", oldElement->name);
    fprintf(fileHandle, "old coords: %s:%d-%d\t", oldElement->chrom, oldElement->start, oldElement->end);
    fprintf(fileHandle, "new coords: %s:%d-%d\n", newElement->chrom, newElement->start, newElement->end);
    fprintf(fileHandle, "old locType = %s, new locType = %s\t", oldElement->locType, newElement->locType);
    fprintf(fileHandle, "old class = %s, new class = %s\n", oldElement->class, newElement->class);
    }

}

void processSnps(struct snpSubsetList *oldTableList, struct hash *newTableHash)
/* loop through oldTableHash */
/* compare to newTableHash */
/* if SNP missing from newTableHash, don't worry about it */
{
struct snpSubsetList *listPtr = NULL;
struct snpSubset *newSubset = NULL;
struct hashEl *helNew = NULL;

FILE *outputFileHandle = mustOpen("snpCompare.out", "w");

verbose(1, "process SNPs...\n");
for (listPtr = oldTableList; listPtr != NULL; listPtr = listPtr->next)
    {
    helNew = hashLookup(newTableHash, listPtr->name);
    if (helNew == NULL) continue;
    newSubset = (struct snpSubset *)helNew->val;
    compareAndLog(outputFileHandle, listPtr, newSubset); 
    }

carefulClose(&outputFileHandle);
}


int main(int argc, char *argv[])
/* work with singly aligning SNPs only */
/* load oldTable subset into list */
/* load newTable subset into hash */
{

char *snpDb = NULL;
struct snpSubsetList *oldTableList = NULL;
struct hash *newTableHash = NULL;
char *oldTableName = NULL;
char *newTableName = NULL;

if (argc != 4)
    usage();

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
processSnps(oldTableList, newTableHash);

return 0;
}
