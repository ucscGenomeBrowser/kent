/* snpCompareLoctype - compare locType old and new snp tables. */
/* Generate counts */
/* Check for changed coords in exact, between and range. */

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

FILE *countFileHandle;
FILE *exactFileHandle;
FILE *betweenFileHandle;
FILE *rangeFileHandle;

int exactToExact;
int exactToBetween;
int exactToRange;
int betweenToBetween;
int betweenToExact;
int betweenToRange;
int rangeToRange;
int rangeToBetween;
int rangeToExact;
int oldToNew;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCompareLoctype - compare old and new snp tables\n"
    "must be in the same database\n"
    "usage:\n"
    "    snpCompareLoctype snpDb oldTable newTable\n");
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

void updateCounters(struct snpSubsetList *oldElement, struct snpSubset *newElement)
{
if (sameString(oldElement->locType, "exact"))
    {
    if (sameString(newElement->locType, "exact"))
        {
	exactToExact++;
	return;
	}
    if (sameString(newElement->locType, "between"))
        {
	exactToBetween++;
	return;
	}
    if (sameString(newElement->locType, "range"))
        {
	exactToRange++;
	return;
	}
    }

if (sameString(oldElement->locType, "between"))
    {
    if (sameString(newElement->locType, "exact"))
        {
	betweenToExact++;
	return;
	}
    if (sameString(newElement->locType, "between"))
        {
	betweenToBetween++;
	return;
	}
    if (sameString(newElement->locType, "range"))
        {
	betweenToRange++;
	return;
	}
    }


if (sameString(oldElement->locType, "range"))
    {
    if (sameString(newElement->locType, "exact"))
        {
	rangeToExact++;
	return;
	}
    if (sameString(newElement->locType, "between"))
        {
	rangeToBetween++;
	return;
	}
    if (sameString(newElement->locType, "range"))
        {
	rangeToRange++;
	return;
	}
    }

if (sameString(newElement->locType, "rangeSubstitution")) oldToNew++;
if (sameString(newElement->locType, "rangeInsertion")) oldToNew++;
if (sameString(newElement->locType, "rangeDeletion")) oldToNew++;

}


void compareAndLog(struct snpSubsetList *oldElement, struct snpSubset *newElement)
{
FILE *fileHandle = NULL;
int distance = 0;
int observedLengthOld = 0;
int observedLengthNew = 0;
int coordSpanOld = 0;
int coordSpanNew = 0;

updateCounters(oldElement, newElement);
if (sameString(oldElement->locType, "between"))
    {
    if (!sameString(newElement->locType, "between")) return;
    fileHandle = betweenFileHandle;
    }
else if (sameString(oldElement->locType, "exact"))
    {
    // if (!sameString(oldElement->class, "snp")) return;
    if (!sameString(oldElement->class, "single")) return;
    if (!sameString(newElement->class, "single")) return;
    if (!sameString(newElement->locType, "exact")) return;
    if (oldElement->end != oldElement->start + 1) return;
    if (newElement->end != newElement->start + 1) return;

    fileHandle = exactFileHandle;
    }
else if (sameString(oldElement->locType, "range"))
    {
    // if (!sameString(oldElement->class, "in-del")) return;
    // if (!sameString(newElement->locType, "range")) return;
    if (!sameString(oldElement->class, "deletion")) return;
    if (!sameString(newElement->class, "deletion")) return;
    fileHandle = rangeFileHandle;
    }
else 
    return;


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

    if (sameString(oldElement->locType, "between"))
        {
	distance = oldElement->start - newElement->start;
	if (distance == 1 || distance == -1)
	    fprintf(fileHandle, "off-by-one\t");
	}
        
    if (sameString(oldElement->locType, "exact"))
	fprintf(fileHandle, "distance = %d\t", oldElement->start - newElement->start);
    if (sameString(oldElement->locType, "range"))
        {
	observedLengthOld = strlen(oldElement->observed);
	observedLengthOld = observedLengthOld - 2;
        coordSpanOld = oldElement->end - oldElement->start;
	observedLengthNew = strlen(newElement->observed);
	observedLengthNew = observedLengthNew - 2;
        coordSpanNew = newElement->end - newElement->start;

        if (observedLengthOld != coordSpanOld && observedLengthNew == coordSpanNew)
            fprintf(fileHandle, "(apparent fix)\t");
	else if (observedLengthOld == coordSpanOld && observedLengthNew != coordSpanNew)
            fprintf(fileHandle, "(apparent error)\t");
	}

    fprintf(fileHandle, "old coords: %s:%d-%d\t", oldElement->chrom, oldElement->start, oldElement->end);
    fprintf(fileHandle, "new coords: %s:%d-%d\n", newElement->chrom, newElement->start, newElement->end);
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

verbose(1, "process SNPs...\n");
for (listPtr = oldTableList; listPtr != NULL; listPtr = listPtr->next)
    {
    helNew = hashLookup(newTableHash, listPtr->name);
    if (helNew == NULL) continue;
    newSubset = (struct snpSubset *)helNew->val;
    compareAndLog(listPtr, newSubset); 
    }

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

exactToExact = 0;
exactToBetween = 0;
exactToRange = 0;
betweenToBetween = 0;
betweenToExact = 0;
betweenToRange = 0;
rangeToRange = 0;
rangeToBetween = 0;
rangeToExact = 0;
oldToNew = 0;

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

exactFileHandle = mustOpen("snpCompareLoctypeExact.out", "w");
betweenFileHandle = mustOpen("snpCompareLoctypeBetween.out", "w");
rangeFileHandle = mustOpen("snpCompareLoctypeRange.out", "w");
processSnps(oldTableList, newTableHash);
carefulClose(&exactFileHandle);
carefulClose(&betweenFileHandle);
carefulClose(&rangeFileHandle);

countFileHandle = mustOpen("snpCompareLoctypeCounts.out", "w");
fprintf(countFileHandle, "exactToExact = %d\n", exactToExact);
fprintf(countFileHandle, "exactToBetween = %d\n", exactToBetween);
fprintf(countFileHandle, "exactToRange = %d\n", exactToRange);
fprintf(countFileHandle, "betweenToBetween = %d\n", betweenToBetween);
fprintf(countFileHandle, "betweenToExact %d\n", betweenToExact);
fprintf(countFileHandle, "betweenToRange %d\n", betweenToRange);
fprintf(countFileHandle, "rangeToRange = %d\n", rangeToRange);
fprintf(countFileHandle, "rangeToBetween = %d\n", rangeToBetween);
fprintf(countFileHandle, "rangeToExact = %d\n", rangeToExact);
fprintf(countFileHandle, "oldToNew = %d\n", oldToNew);
carefulClose(&countFileHandle);

return 0;
}
