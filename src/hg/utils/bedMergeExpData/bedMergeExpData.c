/* bedMergeExpData - Merge probe position information (bed table) with */
/* an expData table and make a new bed file from that. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "bed.h"
#include "expData.h"

void usage()
/* Explain how to run the program. */
{
errAbort("bedMergeExpData - Merge probe position information (bed table) with\n"
	 "an expData table and make a new bed file from that.\n"
	 "usage:\n"
	 "   bedMergeExpData database.expDataTable database.bedTable merged.bed");
}

struct hash *getExpHash(char *dbDotTable)
/* Get the expData table from the database and put it into a hash. */
{
char *table = chopPrefix(dbDotTable);
char *db = dbDotTable;
char query[200];
char **row;
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr = NULL;
struct hash *hash = NULL;
sqlSafef(query, ArraySize(query), "select * from %s", table);
sr = sqlGetResult(conn, query);
hash = newHash(15);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct expData *data = expDataLoad(row);
    hashAdd(hash, data->name, data);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return hash;
}

void freeExpHash(struct hash **pHash)
/* Free up the hash of expDatas. */
{
struct hashEl *list = hashElListHash(*pHash);
struct hashEl *cur;
for (cur = list; cur != NULL; cur = cur->next)
    {
    struct expData *thing = cur->val;
    expDataFree(&thing);    
    }
hashElFreeList(&list);
hashFree(pHash);
}

struct slName *idealizedBed12()
/* Make up the typical bed 12 for future comparison. */
{
struct slName *list = NULL;
slNameAddHead(&list, "chrom");
slNameAddHead(&list, "chromStart");
slNameAddHead(&list, "chromEnd");
slNameAddHead(&list, "name");
slNameAddHead(&list, "score");
slNameAddHead(&list, "strand");
slNameAddHead(&list, "thickStart");
slNameAddHead(&list, "thickEnd");
slNameAddHead(&list, "reserved");
slNameAddHead(&list, "blockCount");
slNameAddHead(&list, "blockSizes");
slNameAddHead(&list, "blockStarts");
slReverse(&list);
return list;
}

int countBedFields(struct sqlConnection *conn, char *table, int *binOffset)
/* Count the fields in a table contributing to a bed.  This makes this */
/* function compatible with bed+ data. */
{
struct slName *bedFields = sqlListFields(conn, table);
struct slName *awesome12 = idealizedBed12();
struct slName *field = bedFields;
struct slName *bed;
int numFields = 0;
*binOffset = 0;
if (!bedFields)
    return 0;
if (sameString(bedFields->name, "bin"))
    {
    *binOffset = 1;
    field = bedFields->next;
    }
for (bed = awesome12; (field != NULL && bed != NULL); field = field->next, bed = bed->next)
/* Loop through 2 lists at the same time, and skip the bin field if */ 
/* it's there. */
    {
    if (!sameString(field->name, bed->name) && !(sameString(field->name, "itemRgb") && (sameString(bed->name, "reserved"))))
	/* the name at the current place in both lists should be the same.  Make an exception */
	/* for "itemRgb" and "reserved", which are both at column 9. */
	break;
    numFields++;
    }
slNameFreeList(&bedFields);
slNameFreeList(&awesome12);
return numFields;
}

void fillInBedALittle(struct bed **pList, int numFields)
/* If it's less than bed 12, make it bed 12. */
{
int i = 1;
struct bed *cur;
for (cur = *pList; cur != NULL; cur = cur->next)
    {
    /* it better be bigger than bed 3. */
    if (numFields < 4)
	{
	char name[50];
	safef(name, ArraySize(name), "item.%d", i+1);
	cur->name = cloneString(name);
	}
    if (numFields < 5)
	cur->score = 1000;
    if (numFields < 6)
	{
	cur->strand[0] = '?';
	cur->strand[1] = '\0';
	}
    if (numFields < 8)
	{
	cur->thickStart = cur->chromStart;
	cur->thickEnd = cur->chromEnd;
	}
    if (numFields < 9)
	cur->itemRgb = 0;
    if (numFields < 12)
	{
	cur->blockSizes = needMem(sizeof(int));
	cur->chromStarts = needMem(sizeof(int));
	cur->blockCount = 1;
	cur->chromStarts[0] = 0;
	cur->blockSizes[0] = cur->chromEnd - cur->chromStart;
	}
    i++;
    }
}

struct bed *bed12DbLoad(char *db, char *table)
/* Load a bed table, and no matter how big it was in the DB, make it bed 12. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr = NULL;
struct bed *bedList = NULL;
char query[200];
char **row;
int binOffset = 0;
int numFields = countBedFields(conn, table, &binOffset);
sqlSafef(query, ArraySize(query), "select * from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *onebed = bedLoadN(row + binOffset, numFields);
    slAddHead(&bedList, onebed);
    }
slReverse(&bedList);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
fillInBedALittle(&bedList, numFields);
return bedList;
}

int *ascendingIntArray(int size)
/* return an array of ints like 0,1,2, ... , size-1. */
{
int *nums = needMem(size * sizeof(int));
int i;
for (i = 0; i < size; i++)
    nums[i] = i;
return nums;
}

void bedInAndOut(struct hash *expHash, char *dbDotBed, char *outBedName)
/* The crux of the program.  Read in the bed table from the DB and merge it */
/* with the expression hash.  Then output the new bed. */
{
char *table = chopPrefix(dbDotBed);
char *db = dbDotBed;
struct bed *bedList = bed12DbLoad(db, table);
struct bed *onebed;
FILE *output = mustOpen(outBedName, "w");
for (onebed = bedList; onebed != NULL; onebed = onebed->next)
    /* For each probe, fetch the experimental data and add it to the bed */
    /* 12 to make a bed 15. */
    {
    struct expData *exps = hashFindVal(expHash, onebed->name);
    /* skip it if there's no experimental data. */
    /* eventually this part could be changed so that it fills in with "missing */
    /* data" values (typically a large, bogus-looking constant) optionally.    */
    if (!exps)
	continue;
    onebed->expCount = exps->expCount;
    onebed->expIds = ascendingIntArray(onebed->expCount);
    onebed->expScores = cloneMem(exps->expScores, sizeof(float) * onebed->expCount);
    bedTabOutN(onebed, 15, output);
    }
bedFreeList(&bedList);
carefulClose(&output);
}

int main(int argc, char *argv[])
/* Check command integrity and run the program. */
{
struct hash *expHash = NULL;
/* struct bed *bedList; */
if (argc != 4)
    usage();
expHash = getExpHash(argv[1]);
bedInAndOut(expHash, argv[2], argv[3]);
freeExpHash(&expHash);
return 0;
}
