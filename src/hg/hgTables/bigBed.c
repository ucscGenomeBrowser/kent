/* bigBed - stuff to handle bigBed in the Table Browser. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "localmem.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "bed.h"
#include "hdb.h"
#include "trackDb.h"
#include "obscure.h"
#include "hmmstats.h"
#include "correlate.h"
#include "asParse.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "hubConnect.h"
#include "asFilter.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: bigBed.c,v 1.11 2010/05/21 23:45:38 braney Exp $";

boolean isBigBed(char *database, char *table, struct trackDb *parent, 
	struct customTrack *(*ctLookupName)(char *table))
/* Local test to see if something is big bed.  Handles hub tracks unlike hIsBigBed. */
{
if (isHubTrack(table))
    {
    struct trackDb *tdb = hashMustFindVal(fullTrackAndSubtrackHash, table);
    return startsWithWord("bigBed", tdb->type);
    }
else
    return hIsBigBed(database, table, parent, ctLookupName);
}

char *bigBedFileName(char *table, struct sqlConnection *conn)
/* Return file name associated with bigBed.  This handles differences whether it's
 * a custom or built-in track.  Do a freeMem on returned string when done. */
{
/* Implementation is same as bigWig. */
return bigWigFileName(table, conn);
}

struct hash *asColumnHash(struct asObject *as)
/* Return a hash full of the object's columns, keyed by colum name */
{
struct hash *hash = hashNew(6);
struct asColumn *col;
for (col = as->columnList; col != NULL; col = col->next)
    hashAdd(hash, col->name, col);
return hash;
}

static void fillField(struct hash *colHash, char *key, char output[HDB_MAX_FIELD_STRING])
/* If key is in colHash, then copy key to output. */
{
if (hashLookup(colHash, key))
    strncpy(output, key, HDB_MAX_FIELD_STRING-1);
}

static struct asObject *bigBedAsOrDefault(struct bbiFile *bbi)
/* Get asObject associated with bigBed - if none exists in file make it up from field counts. */
{
struct asObject *as = bigBedAs(bbi);
if (as == NULL) 
    as = asParseText(bedAsDef(bbi->definedFieldCount, bbi->fieldCount));
return as;
}

struct asObject *bigBedAsForTable(char *table, struct sqlConnection *conn)
/* Get asObject associated with bigBed table. */
{
char *fileName = bigBedFileName(table, conn);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct asObject *as = bigBedAsOrDefault(bbi);
bbiFileClose(&bbi);
freeMem(fileName);
return as;
}

struct hTableInfo *bigBedToHti(char *table, struct sqlConnection *conn)
/* Get fields of bigBed into hti structure. */
{
/* Get columns in asObject format. */
char *fileName = bigBedFileName(table, conn);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct asObject *as = bigBedAsOrDefault(bbi);

/* Allocate hTableInfo structure and fill in info about bed fields. */
struct hash *colHash = asColumnHash(as);
struct hTableInfo *hti;
AllocVar(hti);
hti->rootName = cloneString(table);
hti->isPos= TRUE;
fillField(colHash, "chrom", hti->chromField);
fillField(colHash, "chromStart", hti->startField);
fillField(colHash, "chromEnd", hti->endField);
fillField(colHash, "name", hti->nameField);
fillField(colHash, "score", hti->scoreField);
fillField(colHash, "strand", hti->strandField);
fillField(colHash, "thickStart", hti->cdsStartField);
fillField(colHash, "thickEnd", hti->cdsEndField);
fillField(colHash, "blockCount", hti->countField);
fillField(colHash, "chromStarts", hti->startsField);
fillField(colHash, "blockSizes", hti->endsSizesField);
hti->hasCDS = (bbi->definedFieldCount >= 8);
hti->hasBlocks = (bbi->definedFieldCount >= 12);
char type[256];
safef(type, sizeof(type), "bed %d %c", bbi->definedFieldCount,
	(bbi->definedFieldCount == bbi->fieldCount ? '.' : '+'));
hti->type = cloneString(type);

freeMem(fileName);
hashFree(&colHash);
bbiFileClose(&bbi);
return hti;
}

struct slName *bigBedGetFields(char *table, struct sqlConnection *conn)
/* Get fields of bigBed as simple name list. */
{
char *fileName = bigBedFileName(table, conn);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct asObject *as = bigBedAsOrDefault(bbi);
struct slName *names = asColNames(as);
freeMem(fileName);
bbiFileClose(&bbi);
return names;
}

struct sqlFieldType *bigBedListFieldsAndTypes(char *table, struct sqlConnection *conn)
/* Get fields of bigBed as list of sqlFieldType. */
{
char *fileName = bigBedFileName(table, conn);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct asObject *as = bigBedAsOrDefault(bbi);
struct sqlFieldType *list = sqlFieldTypesFromAs(as);
freeMem(fileName);
bbiFileClose(&bbi);
return list;
}


static void addFilteredBedsOnRegion(struct bbiFile *bbi, struct region *region, 
	char *table, struct asFilter *filter, struct lm *bedLm, struct bed **pBedList)
/* Add relevant beds in reverse order to pBedList */
{
struct lm *bbLm = lmInit(0);
struct bigBedInterval *ivList = NULL, *iv;
ivList = bigBedIntervalQuery(bbi, region->chrom, region->start, region->end, 0, bbLm);
char *row[bbi->fieldCount];
char startBuf[16], endBuf[16];
for (iv = ivList; iv != NULL; iv = iv->next)
    {
    bigBedIntervalToRow(iv, region->chrom, startBuf, endBuf, row, bbi->fieldCount);
    if (asFilterOnRow(filter, row))
        {
	struct bed *bed = bedLoadN(row, bbi->definedFieldCount);
	struct bed *lmBed = lmCloneBed(bed, bedLm);
	slAddHead(pBedList, lmBed);
	bedFree(&bed);
	}
    }

lmCleanup(&bbLm);
}

struct bed *bigBedGetFilteredBedsOnRegions(struct sqlConnection *conn, 
	char *db, char *table, struct region *regionList, struct lm *lm, 
	int *retFieldCount)
/* Get list of beds from bigBed, in all regions, that pass filtering. */
{
/* Connect to big bed and get metadata and filter. */
char *fileName = bigBedFileName(table, conn);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct asObject *as = bigBedAsOrDefault(bbi);
struct asFilter *filter = asFilterFromCart(cart, db, table, as);

/* Get beds a region at a time. */
struct bed *bedList = NULL;
struct region *region;
for (region = regionList; region != NULL; region = region->next)
    addFilteredBedsOnRegion(bbi, region, table, filter, lm, &bedList);
slReverse(&bedList);

/* Clean up and return. */
if (retFieldCount != NULL) 
	*retFieldCount = bbi->definedFieldCount;
bbiFileClose(&bbi);
freeMem(fileName);
return bedList;
}

void bigBedTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f)
/* Print out selected fields from Big Bed.  If fields is NULL, then print out all fields. */
{
if (f == NULL)
    f = stdout;

/* Convert comma separated list of fields to array. */
int fieldCount = chopByChar(fields, ',', NULL, 0);
char **fieldArray;
AllocArray(fieldArray, fieldCount);
chopByChar(fields, ',', fieldArray, fieldCount);

/* Get list of all fields in big bed and turn it into a hash of column indexes keyed by
 * column name. */
struct hash *fieldHash = hashNew(0);
struct slName *bb, *bbList = bigBedGetFields(table, conn);
int i;
for (bb = bbList, i=0; bb != NULL; bb = bb->next, ++i)
    hashAddInt(fieldHash, bb->name, i);

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

/* Open up bigBed file. */
char *fileName = bigBedFileName(table, conn);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct asObject *as = bigBedAsOrDefault(bbi);
struct asFilter *filter = NULL;

if (anyFilter())
    {
    filter = asFilterFromCart(cart, db, table, as);
    if (filter)
        {
	fprintf(f, "# Filtering on %d columns\n", slCount(filter->columnList));
	}
    }

/* Loop through outputting each region */
struct region *region, *regionList = getRegions();
for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(0);
    struct bigBedInterval *iv, *ivList = bigBedIntervalQuery(bbi, region->chrom,
    	region->start, region->end, 0, lm);
    char *row[bbi->fieldCount];
    char startBuf[16], endBuf[16];
    for (iv = ivList; iv != NULL; iv = iv->next)
        {
	bigBedIntervalToRow(iv, region->chrom, startBuf, endBuf, row, bbi->fieldCount);
	if (asFilterOnRow(filter, row))
	    {
	    int i;
	    fprintf(f, "%s", row[columnArray[0]]);
	    for (i=1; i<fieldCount; ++i)
		fprintf(f, "\t%s", row[columnArray[i]]);
	    fprintf(f, "\n");
	    }
	}
    lmCleanup(&lm);
    }

/* Clean up and exit. */
bbiFileClose(&bbi);
hashFree(&fieldHash);
freeMem(fieldArray);
freeMem(columnArray);
}

void showSchemaBigBed(char *table)
/* Show schema on bigBed. */
{
/* Figure out bigBed file name and open it.  Get contents for first chromosome as an example. */
struct sqlConnection *conn = hAllocConn(database);
char *fileName = bigBedFileName(table, conn);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct bbiChromInfo *chromList = bbiChromList(bbi);
struct lm *lm = lmInit(0);
struct bigBedInterval *ivList = bigBedIntervalQuery(bbi, chromList->name, 0, 
					 	    chromList->size, 10, lm);

/* Get description of columns, making it up from BED records if need be. */
struct asObject *as = bigBedAs(bbi);
if (as == NULL)
    as = asParseText(bedAsDef(bbi->definedFieldCount, bbi->fieldCount));

hPrintf("<B>Database:</B> %s", database);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Primary Table:</B> %s<br>", table);
hPrintf("<B>Big Bed File:</B> %s", fileName);
if (bbi->version >= 2)
    {
    hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Item Count:</B> ");
    printLongWithCommas(stdout, bigBedItemCount(bbi));
    }
hPrintf("<BR>\n");
hPrintf("<B>Format description:</B> %s<BR>", as->comment);

/* Put up table that describes fields. */
hTableStart();
hPrintf("<TR><TH>field</TH>");
hPrintf("<TH>example</TH>");
hPrintf("<TH>description</TH> ");
puts("</TR>\n");
struct asColumn *col;
int colCount = 0;
char *row[bbi->fieldCount];
char startBuf[16], endBuf[16];
char *dupeRest = lmCloneString(lm, ivList->rest);	/* Manage rest-stomping side-effect */
bigBedIntervalToRow(ivList, chromList->name, startBuf, endBuf, row, bbi->fieldCount);
ivList->rest = dupeRest;
for (col = as->columnList; col != NULL; col = col->next)
    {
    hPrintf("<TR><TD><TT>%s</TT></TD>", col->name);
    hPrintf("<TD>%s</TD>", row[colCount]);
    hPrintf("<TD>%s</TD></TR>", col->comment);
    ++colCount;
    }

/* If more fields than descriptions put up minimally helpful info (at least has example). */
for ( ; colCount < bbi->fieldCount; ++colCount)
    {
    hPrintf("<TR><TD><TT>column%d</TT></TD>", colCount+1);
    hPrintf("<TD>%s</TD>", row[colCount]);
    hPrintf("<TD>n/a</TD></TR>\n");
    }
hTableEnd();


/* Put up another section with sample rows. */
webNewSection("Sample Rows");
hTableStart();

/* Print field names as column headers for example */
hPrintf("<TR>");
int colIx = 0;
for (col = as->columnList; col != NULL; col = col->next)
    {
    hPrintf("<TH>%s</TH>", col->name);
    ++colIx;
    }
for (; colIx < colCount; ++colIx)
    hPrintf("<TH>column%d</TH>", colIx+1);
hPrintf("</TR>\n");

/* Print sample lines. */
struct bigBedInterval *iv;
for (iv=ivList; iv != NULL; iv = iv->next)
    {
    bigBedIntervalToRow(iv, chromList->name, startBuf, endBuf, row, bbi->fieldCount);
    hPrintf("<TR>");
    for (colIx=0; colIx<colCount; ++colIx)
        {
	writeHtmlCell(row[colIx]);
	}
    hPrintf("</TR>\n");
    }
hTableEnd();

/* Clean up and go home. */
lmCleanup(&lm);
bbiFileClose(&bbi);
freeMem(fileName);
hFreeConn(&conn);
}
