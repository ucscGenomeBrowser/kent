/* longRange - stuff to handle long range interaction stuff in table browser. */

/* Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgTables.h"
#include "hubConnect.h"
#include "longRange.h"
#include "asFilter.h"
#include "bedTabix.h"

boolean isLongTabixTable(char *table)
/* Return TRUE if table corresponds to a longTabix file. */
{
if (isHubTrack(table))
    {
    struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
    return startsWithWord("bedTabix", tdb->type) || startsWithWord("longTabix", tdb->type);
    }
else
    return trackIsType(database, table, curTrack, "longTabix", ctLookupName);
}

struct slName *getLongTabixFields()
/* Get fields of bam as simple name list. */
{
struct asObject *as = longTabixAsObj();
return asColNames(as);
}

struct sqlFieldType *longTabixListFieldsAndTypes()
/* Get fields of bigBed as list of sqlFieldType. */
{
struct asObject *as = longTabixAsObj();
return sqlFieldTypesFromAs(as);
}

struct hTableInfo *longTabixToHti(char *table)
/* Get standard fields of longTabix into hti structure. */
{
struct hTableInfo *hti;
AllocVar(hti);
hti->rootName = cloneString(table);
hti->isPos= TRUE;
strcpy(hti->chromField, "chrom");
strcpy(hti->startField, "chromStart");
strcpy(hti->nameField, "chromEnd");
hti->type = cloneString("longTabix");
return hti;
}

void longTabixTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f)
/* Print out selected fields from long tabix.  If fields is NULL, then print out all fields. */
{
struct hTableInfo *hti = NULL;
hti = getHti(db, table, conn);
struct hash *idHash = NULL;
char *idField = getIdField(db, curTrack, table, hti);
int idFieldNum = 0;

/* if we know what field to use for the identifiers, get the hash of names */
if (idField != NULL)
    idHash = identifierHash(db, table);

if (f == NULL)
    f = stdout;

/* Convert comma separated list of fields to array. */
int fieldCount = chopByChar(fields, ',', NULL, 0);
char **fieldArray;
AllocArray(fieldArray, fieldCount);
chopByChar(fields, ',', fieldArray, fieldCount);

/* Get list of all fields in long tabix and turn it into a hash of column indexes keyed by
 * column name. */
struct hash *fieldHash = hashNew(0);
struct slName *bb, *bbList = getLongTabixFields();
int i;
for (bb = bbList, i=0; bb != NULL; bb = bb->next, ++i)
    {
    /* if we know the field for identifiers, save it away */
    if ((idField != NULL) && sameString(idField, bb->name))
	idFieldNum = i;
    hashAddInt(fieldHash, bb->name, i);
    }

/* Create an array of column indexes corresponding to the selected field list. */
int *columnArray;
AllocArray(columnArray, fieldCount);
for (i=0; i<fieldCount; ++i)
    {
    columnArray[i] = hashIntVal(fieldHash, fieldArray[i]);
    }

/* Output row of labels */
fprintf(f, "#%s", fieldArray[0]);
for (i=1; i<fieldCount; ++i)
    fprintf(f, "\t%s", fieldArray[i]);
fprintf(f, "\n");

struct asObject *as = longTabixAsObj();
struct asFilter *filter = NULL;

if (anyFilter())
    {
    filter = asFilterFromCart(cart, db, table, as);
    if (filter)
        {
	fprintf(f, "# Filtering on %d columns\n", slCount(filter->columnList));
	}
    }

struct region *region, *regionList = getRegions();
int maxOut = bigFileMaxOutput();
char *fileName = bigFileNameFromCtOrHub(table, conn);
struct bedTabixFile *btf = bedTabixFileMayOpen(fileName, NULL, 0, 0);

/* Loop through outputting each region */
for (region = regionList; region != NULL && (maxOut > 0); region = region->next)
    {
    if (!lineFileSetTabixRegion(btf->lf, region->chrom, region->start, region->end))
        continue;
    char *row[6];
    int wordCount;
    while (((wordCount = lineFileChopTab(btf->lf, row)) > 0) && (maxOut > 0))
        {
	if (asFilterOnRow(filter, row))
	    {
	    /* if we're looking for identifiers, check if this matches */
	    if ((idHash != NULL)&&(hashLookup(idHash, row[idFieldNum]) == NULL))
		continue;

	    int i;
	    fprintf(f, "%s", row[columnArray[0]]);
	    for (i=1; i<fieldCount; ++i)
		fprintf(f, "\t%s", row[columnArray[i]]);
	    fprintf(f, "\n");
	    maxOut --;
	    }
	}
    freeMem(fileName);
    }

if (maxOut == 0)
    errAbort("Reached output limit of %d data values, please make region smaller,\n\tor set a higher output line limit with the filter settings.", bigFileMaxOutput());
/* Clean up and exit. */
hashFree(&fieldHash);
freeMem(fieldArray);
freeMem(columnArray);
}

static void addFilteredBedsOnRegion(char *fileName, struct region *region,
	char *table, struct asFilter *filter, struct lm *bedLm, struct bed **pBedList,
	struct hash *idHash, int *pMaxOut)
/* Add relevant beds in reverse order to pBedList */
{
struct bedTabixFile *btf = bedTabixFileMayOpen(fileName, NULL, 0, 0);
if (!lineFileSetTabixRegion(btf->lf, region->chrom, region->start, region->end))
    return;
char *row[6];
int wordCount;
while (((wordCount = lineFileChopTab(btf->lf, row)) > 0) && (*pMaxOut > 0))
    {
    if (asFilterOnRow(filter, row))
        {
	if ((idHash != NULL) && (hashLookup(idHash, row[3]) == NULL))
	    continue;

	struct bed *bed;
	lmAllocVar(bedLm, bed);
	bed->chrom = cloneString(row[0]);
	bed->chromStart = sqlUnsigned(row[1]);
	bed->chromEnd = sqlUnsigned(row[2]);
	bed->name = cloneString(row[3]);
	}
    (*pMaxOut)--;
    if (*pMaxOut <= 0)
	break;
    }
}

struct bed *longTabixGetFilteredBedsOnRegions(struct sqlConnection *conn,
	char *db, char *table, struct region *regionList, struct lm *lm,
	int *retFieldCount)
/* Get list of beds from long tabix, in all regions, that pass filtering. */
{
int maxOut = bigFileMaxOutput();
/* Figure out bam file name get column info and filter. */
struct asObject *as = longTabixAsObj();
struct asFilter *filter = asFilterFromCart(cart, db, table, as);
struct hash *idHash = identifierHash(db, table);

/* Get beds a region at a time. */
struct bed *bedList = NULL;
struct region *region;
char *fileName = bigFileNameFromCtOrHub(table, conn);
for (region = regionList; region != NULL; region = region->next)
    {
    addFilteredBedsOnRegion(fileName, region, table, filter, lm, &bedList, idHash, &maxOut);
    freeMem(fileName);
    if (maxOut <= 0)
	{
	errAbort("Reached output limit of %d data values, please make region smaller,\n"
	     "\tor set a higher output line limit with the filter settings.", bigFileMaxOutput());
	}
    }
slReverse(&bedList);
return bedList;
}

void showSchemaLongTabix(char *table, struct trackDb *tdb)
/* Show schema on long tabix. */
// FIXME: restore 'View schema' and 'schema' links for longTabix type (in hui.c) 
//      when this is implemented
{
}
