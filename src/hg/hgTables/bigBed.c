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
#include "hgTables.h"

static char const rcsid[] = "$Id: bigBed.c,v 1.11 2010/05/21 23:45:38 braney Exp $";


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
uglyf("ok 5.1.1.1<BR>\n");
char *fileName = bigBedFileName(table, conn);
uglyf("ok 5.1.1.2 fileName=%s<BR>\n", fileName);
struct bbiFile *bbi = bigBedFileOpen(fileName);
uglyf("ok 5.1.1.3<BR>\n");
struct asObject *as = bigBedAsOrDefault(bbi);
uglyf("ok 5.1.1.4<BR>\n");

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
uglyf("ok 5.1.1.5<BR>\n");

freeMem(fileName);
hashFree(&colHash);
bbiFileClose(&bbi);
uglyf("ok 5.1.1.6<BR>\n");
return hti;
}

struct slName *asColNames(struct asObject *as)
/* Get list of column names. */
{
struct slName *list = NULL, *el;
struct asColumn *col;
for (col = as->columnList; col != NULL; col = col->next)
    {
    el = slNameNew(col->name);
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
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

struct sqlFieldType *sqlFieldTypesFromAs(struct asObject *as)
/* Convert asObject to list of sqlFieldTypes */
{
struct sqlFieldType *ft, *list = NULL;
struct asColumn *col;
for (col = as->columnList; col != NULL; col = col->next)
    {
    struct dyString *type = asColumnToSqlType(col);
    ft = sqlFieldTypeNew(col->name, type->string);
    slAddHead(&list, ft);
    dyStringFree(&type);
    }
slReverse(&list);
return list;
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


enum asFilterDataType
/* High level data type. */
   {
   afdtNone = 0,
   afdtString = 1,
   afdtLong = 2,
   afdtDouble = 3,
   afdtChar = 4,
   };

struct asLongFilter
/* Filter on long value */
   {
   enum numericFilterType op;
   long long *thresholds;
   };

struct asDoubleFilter
/* Filter on double value */
   {
   enum numericFilterType op;
   double *thresholds;
   };

struct asCharFilter
/* Filter on a char value */
    {
    enum charFilterType op;
    char *matches;
    boolean invert;
    };

struct asStringFilter
/* Filter on a string value */
    {
    enum stringFilterType op;
    char **matches;
    boolean invert;
    };

void asCharFilterFree(struct asCharFilter **pFilter)
/* Free up memory associated with filter. */
{
struct asCharFilter *filter = *pFilter;
if (filter != NULL)
    {
    freeMem(filter->matches);
    freez(pFilter);
    }
}

void asStringFilterFree(struct asStringFilter **pFilter)
/* Free up memory associated with filter. */
{
struct asStringFilter *filter = *pFilter;
if (filter != NULL)
    {
    freeMem(filter->matches);
    freez(pFilter);
    }
}

struct asDoubleFilter *asDoubleFilterFromCart(struct cart *cart, char *fieldPrefix)
/* Get filter settings for double out of cart. */
{
struct asDoubleFilter *filter = NULL;
char varName[256];
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterCmpVar);
char *cmp = cartOptionalString(cart, varName);
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterPatternVar);
char *pat = cartOptionalString(cart, varName);
if (!isEmpty(cmp) && !isEmpty(pat))
    {
    AllocVar(filter);
    cgiToDoubleFilter(cmp, pat, &filter->op, &filter->thresholds);
    }
return filter;
}

struct asLongFilter *asLongFilterFromCart(struct cart *cart, char *fieldPrefix)
/* Get filter settings for double out of cart. */
{
struct asLongFilter *filter = NULL;
char varName[256];
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterCmpVar);
char *cmp = cartOptionalString(cart, varName);
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterPatternVar);
char *pat = cartOptionalString(cart, varName);
if (!isEmpty(cmp) && !isEmpty(pat))
    {
    AllocVar(filter);
    cgiToLongFilter(cmp, pat, &filter->op, &filter->thresholds);
    }
return filter;
}

struct asCharFilter *asCharFilterFromCart(struct cart *cart, char *fieldPrefix)
/* Get filter settings for double out of cart. */
{
struct asCharFilter *filter = NULL;
char varName[256];
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterDdVar);
char *dd = cartOptionalString(cart, varName);
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterPatternVar);
char *pat = cartOptionalString(cart, varName);
if (!isEmpty(dd) && !isEmpty(pat))
    {
    AllocVar(filter);
    cgiToCharFilter(dd, pat, &filter->op, &filter->matches, &filter->invert);
    if (filter->op == cftIgnore)	// Filter out nop
	asCharFilterFree(&filter);
    }
return filter;
}

struct asStringFilter *asStringFilterFromCart(struct cart *cart, char *fieldPrefix)
/* Get filter settings for double out of cart. */
{
struct asStringFilter *filter = NULL;
char varName[256];
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterDdVar);
char *dd = cartOptionalString(cart, varName);
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterPatternVar);
char *pat = cartOptionalString(cart, varName);
if (!isEmpty(dd) && !isEmpty(pat))
    {
    AllocVar(filter);
    cgiToStringFilter(dd, pat, &filter->op, &filter->matches, &filter->invert);
    if (filter->op == sftIgnore)	// Filter out nop
	asStringFilterFree(&filter);
    }
return filter;
}

union asFilterData
/* One of the above four. */
    {
    struct asLongFilter *l;
    struct asDoubleFilter *d;
    struct asCharFilter *c;
    struct asStringFilter *s;
    };

struct asFilterColumn
/* A type of filter applied to a column. */
   {
   struct asFilterColumn *next;
   struct asColumn *col;		/* Column we operate on. */
   int colIx;				/* Index of column. */
   enum asFilterDataType dataType;	/* Type of limit parameters. */
   union asFilterData filter;		/* Filter data including op. */
   };

struct asFilter
/* A filter that can be applied to weed out rows in a table with an associated .as file. */
    {
    struct asFilter *next;
    struct asFilterColumn *columnList;  /* A list of column filters to apply */
    };


boolean asFilterString(struct asStringFilter *filter, char *x)
/* Return TRUE if x passes filter. */
{
return bedFilterString(x, filter->op, filter->matches, filter->invert);
}

boolean asFilterLong(struct asLongFilter *filter, long long x)
/* Return TRUE if x passes filter. */
{
return bedFilterLong(x, filter->op, filter->thresholds);
}

boolean asFilterDouble(struct asDoubleFilter *filter, double x)
/* Return TRUE if x passes filter. */
{
return bedFilterDouble(x, filter->op, filter->thresholds);
}

boolean asFilterChar(struct asCharFilter *filter, char x)
/* Return TRUE if x passes filter. */
{
return bedFilterChar(x, filter->op, filter->matches, filter->invert);
}

boolean asFilterOneCol(struct asFilterColumn *filtCol, char *s)
/* Return TRUE if s passes filter. */
{
switch (filtCol->dataType)
    {
    case afdtString:
        return asFilterString(filtCol->filter.s, s);
    case afdtLong:
        return asFilterLong(filtCol->filter.l, atoll(s));
    case afdtDouble:
        return asFilterDouble(filtCol->filter.d, atof(s));
    case afdtChar:
        return asFilterChar(filtCol->filter.c, s[0]);
    default:
        internalErr();
	return FALSE;
    }
}

boolean asFilterOnRow(struct asFilter *filter, char **row)
/* Return TRUE if row passes filter if any. */
{
if (filter != NULL)
    {
    struct asFilterColumn *col;
    for (col = filter->columnList; col != NULL; col = col->next)
	{
	if (!asFilterOneCol(col, row[col->colIx]))
	    return FALSE;
	}
    }
return TRUE;
}

struct asFilter *asFilterFromCart(struct cart *cart, char *db, char *table, struct asObject *as)
/* Examine cart for filter relevant to this table, and create object around it. */
{
/* Get list of filter variables for this table. */
char tablePrefix[128], fieldPrefix[192];
safef(tablePrefix, sizeof(tablePrefix), "%s%s.%s.", hgtaFilterVarPrefix, db, table);

struct asFilter *asFilter;
AllocVar(asFilter);

struct asColumn *col;
int colIx = 0;
for (col = as->columnList; col != NULL; col = col->next, ++colIx)
    {
    safef(fieldPrefix, sizeof(fieldPrefix), "%s%s.", tablePrefix, col->name);
    struct asTypeInfo *lt = col->lowType;
    union asFilterData lowFilter;
    enum asFilterDataType dataType = afdtNone;	
    lowFilter.d = NULL;
    switch (lt->type)
	{
	case t_double:
	case t_float:
	    lowFilter.d = asDoubleFilterFromCart(cart, fieldPrefix);
	    dataType = afdtDouble;
	    break;
	case t_char:
	    lowFilter.c = asCharFilterFromCart(cart, fieldPrefix);
	    dataType = afdtChar;
	    break;
	case t_int:
	case t_uint:
	case t_short:
	case t_ushort:
	case t_byte:
	case t_ubyte:
	case t_off:
	    lowFilter.l = asLongFilterFromCart(cart, fieldPrefix);
	    dataType = afdtLong;
	    break;
	case t_string:
	case t_lstring:
	    lowFilter.s = asStringFilterFromCart(cart, fieldPrefix);
	    dataType = afdtString;
	    break;
	case t_object:
	case t_simple:
	case t_enum:
	case t_set:
	default:
	    internalErr();
	    break;
	}
    if (lowFilter.d != NULL)
        {
	struct asFilterColumn *colFilt;
	AllocVar(colFilt);
	colFilt->col = col;
	colFilt->colIx = colIx;
	colFilt->dataType = dataType;
	colFilt->filter = lowFilter;
	slAddHead(&asFilter->columnList, colFilt);
	}
    }
slReverse(&asFilter->columnList);
return asFilter;
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
