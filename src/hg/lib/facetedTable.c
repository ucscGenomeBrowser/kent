/* facetedTable - routines to help produce a sortable table with facet selection fields.
 * This builds on top of things in tablesTables and facetField. */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "web.h"
#include "trashDir.h"
#include "hCommon.h"
#include "htmlColor.h"
#include "hgColors.h"
#include "fieldedTable.h"
#include "tablesTables.h"
#include "facetField.h"
#include "hgConfig.h"
#include "facetedTable.h"

/* These two may someday need to be non-constant */
#define ftClusterOutIx 0    /* Output for synthesized cluster name is always in zero column */
#define ftKeySep '/'	    /* Componenets of composite keys are separated with they */

static struct facetedTable *facetedTableNew(char *name, char *varPrefix, char *facets)
/* Return new, mostly empty faceted table */
{
struct facetedTable *facTab;
AllocVar(facTab);
facTab->name = cloneString(name);
facTab->varPrefix = cloneString(varPrefix);
facTab->facets = cloneString(facets);
return facTab;
}

struct facetedTable *facetedTableFromTable(struct fieldedTable *table, 
    char *varPrefix, char *facets)
/* Construct a facetedTable around a fieldedTable */
{
struct facetedTable *facTab = facetedTableNew(table->name, varPrefix, facets);
facTab->table = table;
AllocArray(facTab->ffArray, table->fieldCount);
return facTab;
}


void facetedTableFree(struct facetedTable **pFt)
/* Free up resources associated with faceted table */
{
struct facetedTable *facTab = *pFt;
if (facTab != NULL)
    {
    freeMem(facTab->name);
    freeMem(facTab->varPrefix);
    freeMem(facTab->facets);
    freeMem(facTab->ffArray);
    freez(pFt);
    }
}

static void facetedTableWebInit()
/* Print out scripts and css that we need.  We should be in a page body or title. */
{
static boolean initted = FALSE;
if (initted)
    return;
initted = TRUE;
webIncludeResourceFile("facets.css");
printf("\t\t<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js\"></script>");
printf("\t\t<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\"\n"
    "\t\t integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\"\n"
    "\t\t crossorigin=\"anonymous\">\n"
    "\n"
    "\t\t<!-- Latest compiled and minified CSS -->\n"
    "\t\t<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css\"\n"
    "\t\t integrity=\"sha384-Gn5384xqQ1aoWXA+058RXPxPg6fy4IWvTNh0E263XmFcJlSAwiGgFAW/dAiS6JXm\"\n"
    "\t\t crossorigin=\"anonymous\">\n"
    "\n"
    "\t\t<!-- Latest compiled and minified JavaScript -->\n"
    "\t\t<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js\"\n"
    "\t\t integrity=\"sha384-JZR6Spejh4U02d8jOt6vLEHfe/JQGiRRSQQxSfFWpi1MquVdAyjUar5+76PVCmYl\"\n"
    "\t\t crossorigin=\"anonymous\"></script>\n"
    );
}

static char *facetedTableSelList(struct facetedTable *facTab, struct cart *cart)
/* Look up list of selected items in facets from cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_selList", facTab->varPrefix);
return cartOptionalString(cart, var);
}

static char *facetedTableSelOp(struct facetedTable *facTab, struct cart *cart)
/* Look up selOp in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_op", facTab->varPrefix);
return cartOptionalString(cart, var);
}

static char *facetedTableSelField(struct facetedTable *facTab, struct cart *cart)
/* Look up sel field in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_fieldName", facTab->varPrefix);
return cartOptionalString(cart, var);
}

static char *facetedTableSelVal(struct facetedTable *facTab, struct cart *cart)
/* Look up sel val in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_fieldVal", facTab->varPrefix);
return cartOptionalString(cart, var);
}

static void facetedTableRemoveOpVars(struct facetedTable *facTab, struct cart *cart)
/* Remove sel op/field/name vars from cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_op", facTab->varPrefix);
cartRemove(cart, var);
safef(var, sizeof(var), "%s_facet_fieldVal", facTab->varPrefix);
cartRemove(cart, var);
safef(var, sizeof(var), "%s_facet_fieldName", facTab->varPrefix);
cartRemove(cart, var);
}

boolean facetedTableUpdateOnClick(struct facetedTable *facTab, struct cart *cart)
/* If we got called by a click on a facet deal with that and return TRUE, else do
 * nothing and return false */
{
char *selOp = facetedTableSelOp(facTab, cart);
if (selOp)
    {
    char *selFieldName = facetedTableSelField(facTab, cart);
    if (selFieldName != NULL)
	{
	char *selFieldVal = facetedTableSelVal(facTab, cart);
	char selListVar[256];
	safef(selListVar, sizeof(selListVar), "%s_facet_selList", facTab->varPrefix);
	char *selectedFacetValues=cartUsualString(cart, selListVar, "");
	struct facetField *selList = deLinearizeFacetValString(selectedFacetValues);
	selectedListFacetValUpdate(&selList, selFieldName, selFieldVal, selOp);
	char *newSelectedFacetValues = linearizeFacetVals(selList);
	cartSetString(cart, selListVar, newSelectedFacetValues);
	facetedTableRemoveOpVars(facTab, cart);
	}
    return TRUE;
    }
else
    return FALSE;
}


struct mergeGroup
/* Adds up series of counts and values with values weighted by count*/
    {
    struct mergeGroup *next;
    char *key;		     /* Unique key for this group */
    struct fieldedRow *aRow; /* First row we encounter in input table */
    struct slRef *rowRefList;  /* List of refs to all rows (struct fieldedRow *) in merge */
    // struct mergeColHelper *col;  /* Associated column if any */
    };

struct mergeColHelper
/* Helper structure for merging - one per column whether merged or not */
    {
    struct mergeColHelper *next;
    char *name;	/* Name of column */
    int ix;	/* Column index in table */
    boolean isMerged;	/* If true it's a merging column */
    boolean isFacet;	/* If true it's a faceting column */
    boolean sameAfterMerger;  /* If true then all values after merge would be same */
    boolean isCount;	/* Is count field */
    boolean isVal;	/* Is val field */
    boolean isKey;	/* Is it the key field */
    boolean isColor;	/* Is it the color field? */
    int mergedIx;	/* Column index in merged table */
    };

static struct mergeColHelper *makeColList(struct facetedTable *facTab, struct fieldedTable *table,
    struct facetField *selList, struct mergeColHelper **retCountCol, 
    struct mergeColHelper **retValCol, struct mergeColHelper **retColorCol)
/* Create a list of all columns of table annotated with some useful info */
{
/* Build up hash of all fields with selection info */
struct hash *ffHash = hashNew(4);
struct facetField *ff;
for (ff = selList; ff != NULL; ff = ff->next)
    hashAdd(ffHash, ff->fieldName, ff);

/* Build up hash of all faceted fields */
struct hash *facetHash = hashNew(4);
struct slName *facetNameList = slNameListFromComma(facTab->facets);
struct slName *el;
for (el = facetNameList; el != NULL; el = el->next)
    hashAdd(facetHash, el->name, NULL);

/* Stream through columns of table making up of helpers, one for each field  */
struct mergeColHelper *list = NULL, *helper;
int i;
for (i=0; i<table->fieldCount; ++i)
    {
    char *field = table->fields[i];
    AllocVar(helper);
    helper->name = field;
    helper->ix = i;
    struct facetField *ff = hashFindVal(ffHash, field);
    if (ff != NULL)
	helper->isMerged = ff->isMerged;
    helper->isFacet = (hashLookup(facetHash, field) != NULL);
    helper->isVal = sameString(field, "val");
    if (helper->isVal)
        *retValCol = helper;
    helper->isCount = sameString(field, "count");
    if (helper->isCount)
        *retCountCol = helper;
    helper->isColor = sameString(field, "color");
    if (helper->isColor)
        *retColorCol = helper;
    helper->isKey = (i == ftClusterOutIx);
    slAddHead(&list, helper);
    }
slReverse(&list);

/* Clean up and go home */
hashFree(&ffHash);
hashFree(&facetHash);
slNameFreeList(&facetNameList);
return list;
}

static char *calcMergedKey(struct mergeColHelper *colList, char **row, 
    struct dyString *key)
/* Calculate merging key for this row. Puts result in key, whose contents will be overwritten */
{
dyStringClear(key);
struct mergeColHelper *col;
for (col = colList; col != NULL; col = col->next)
    {
    if (col->isFacet && !col->isMerged)
	{
	if (key->stringSize != 0)
	    dyStringAppendC(key, ftKeySep);
	dyStringAppend(key, row[col->ix]);
	}
    }
return key->string;
}

long summedCountAt(int countIx, struct mergeGroup *merge)
/* Return sum of column across merge group */
{
long long total = 0;
struct slRef *ref;
for (ref = merge->rowRefList; ref != NULL; ref = ref->next)
    {
    struct fieldedRow *fr = ref->val;
    char **row = fr->row;
    int count = sqlUnsigned(row[countIx]);
    total += count;
    }
return total;
}

static int scaleColorPart(double colSum, long count)
/* Return component scaled for count but lighten it away if
 * the count is to low. */
{
int maxVal = 240;
int threshold = 100;
if (count == 0)
    return maxVal;  // Which makes us nearly white and invisible
double fullColor = colSum/count;
int underThreshold = threshold - count;
if (underThreshold < 0)
    return round(fullColor);

/* We are under threshold.  Blend us with white (aka maxVal) */
double ratioUnderThreshold = (double)underThreshold/(double)threshold;
return (1 - ratioUnderThreshold)*fullColor + ratioUnderThreshold*maxVal;
}

double weightedAveValAt(int countIx, int valIx, struct mergeGroup *merge)
/* Return wieghted average value */
{
long totalCount = 0;
double weightedSum = 0;
struct slRef *ref;
for (ref = merge->rowRefList; ref != NULL; ref = ref->next)
    {
    struct fieldedRow *fr = ref->val;
    char **row = fr->row;
    int count = sqlUnsigned(row[countIx]);
    double val = sqlDouble(row[valIx]);
    totalCount += count;
    weightedSum += val*count;
    }
return weightedSum/totalCount;
}

double aveValAt(int valIx, struct mergeGroup *merge)
/* Return average value */
{
int count = 0;
double weightedSum = 0;
struct slRef *ref;
for (ref = merge->rowRefList; ref != NULL; ref = ref->next)
    {
    struct fieldedRow *fr = ref->val;
    char **row = fr->row;
    double val = sqlDouble(row[valIx]);
    count += 1;
    weightedSum += val;
    }
return weightedSum/count;
}

void weightedAveColorAt(struct mergeColHelper *countCol, int colorIx, struct mergeGroup *merge, 
    int *retR, int *retG, int *retB)
/* Return wieghted average color.  Assumes color fields all #RRGGBB hex format  */
{
long totalCount = 0;
double rSum = 0, gSum=0, bSum = 0;
struct slRef *ref;
for (ref = merge->rowRefList; ref != NULL; ref = ref->next)
    {
    struct fieldedRow *fr = ref->val;
    char **row = fr->row;
    int count = (countCol != NULL ? sqlUnsigned(row[countCol->ix]) : 1);
    char *hexColor = row[colorIx];
    unsigned htmlColor;	// Format will be 8 bits each of RGB
    if (!htmlColorForCode(hexColor, &htmlColor))
	internalErr();
    int r,g,b;
    htmlColorToRGB(htmlColor, &r, &g, &b);
    rSum += r*count;
    gSum += g*count;
    bSum += b*count;
    totalCount += count;
    }
*retR = scaleColorPart(rSum,totalCount);
*retG = scaleColorPart(gSum,totalCount);
*retB = scaleColorPart(bSum,totalCount);
}


static struct mergeGroup *findMergeGroups(struct fieldedTable *tableIn, 
    struct mergeColHelper *colList)
/* Return list of mergeGroups corresponding to colList */
{
struct mergeGroup *mergeList = NULL;
struct hash *mergeHash = hashNew(0);
struct dyString *keyBuf = dyStringNew(0);
struct fieldedRow *fr;
for (fr = tableIn->rowList; fr != NULL; fr = fr->next)
    {
    char *key = calcMergedKey(colList, fr->row, keyBuf);
    struct mergeGroup *merge = hashFindVal(mergeHash, key);
    if (merge == NULL)
        {
	AllocVar(merge);
	merge->key = cloneString(key);
	merge->aRow = fr;
	hashAdd(mergeHash, key, merge);
	slAddHead(&mergeList, merge);
	}
    refAdd(&merge->rowRefList, fr);
    }

/* Do list reversals, damn cheap singly-linked lists always needing this! */
struct mergeGroup *merge;
for (merge = mergeList; merge != NULL; merge = merge->next)
    slReverse(&merge->rowRefList);
slReverse(&mergeList);

/* Clean up and go home. */
dyStringFree(&keyBuf);
hashFree(&mergeHash);
return mergeList;
}

#ifdef DEBUG
static struct hash *hashColList(struct mergeColHelper *colList)
/* Make hash of colList keyed by name */
{
   { // ugly debug
   struct dyString *dy = dyStringNew(0);
    struct mergeColHelper *col;
    for (col = colList; col != NULL; col = col->next)
	dyStringPrintf(dy, "%s,", col->name);
    uglyAbort("hashColList on %s", dy->string);
   }
struct hash *hash = hashNew(0);
struct mergeColHelper *col;
for (col = colList; col != NULL; col = col->next)    hashAdd(hash, col->name, col);
    hashAdd(hash, col->name, col);
return hash;
}
#endif /* DEBUG */

void annotateColListAndMakeNewFields(struct mergeColHelper *colList, 
    struct mergeGroup *mergeList, int *retCount, struct mergeColHelper ***retNewFields)
/* Update fields in colList. */
{
struct mergeGroup *merge;
struct mergeColHelper *col;

/* Make a trip through merged table noting whether a each column merges cleanly */
for (col = colList; col != NULL; col = col->next)
    {
    int colIx = col->ix;
    boolean mergeOk = TRUE;
    for (merge = mergeList; merge != NULL && mergeOk; merge = merge->next)
        {
	struct slRef *ref = merge->rowRefList;
	struct fieldedRow *fr = ref->val;
	char **row = fr->row;
	char *first = row[colIx];
	for (ref = ref->next; ref != NULL; ref = ref->next)
	    {
	    fr = ref->val;
	    row = fr->row;
	    if (!sameString(first, row[colIx]))
	        {
		mergeOk = FALSE;
		break;
		}
	    }
	}
    col->sameAfterMerger = mergeOk;
    }

/* Figure out list of fields for new table 
 *     We'll keep the first field but repurpose it as new index.
 *     We'll copy over the non-merged facet fields
 *     We'll copy over other fields that have the same value for each row in the merge
 *     We'll compute new count and val fields where count is sum and val is weighted average 
 *     If there's a color field we'll merge it by weighted average as well */

struct mergeColHelper **newFields;
AllocArray(newFields, slCount(colList)); // Big enough
int newFieldCount = 0;
for (col = colList; col != NULL; col = col->next)
    {
    if (col->sameAfterMerger || col->isKey || col->isCount || col->isVal || col->isColor)
        {
	newFields[newFieldCount] = col;
	col->mergedIx = newFieldCount;
	newFieldCount += 1;
	}
    else
        col->mergedIx = -1;
    }
*retCount = newFieldCount;
*retNewFields = newFields;
}

static struct facetField **recomputeFfArray(struct facetField **oldFfArray, 
    struct mergeColHelper *colList, int newFieldCount)
/* Return freshly computed ffArray with some of bits of oldArray skipped */
{
/* Recompute ffArray */
struct facetField **newFfArray;
AllocArray(newFfArray, newFieldCount);
struct mergeColHelper *col;
for (col = colList; col != NULL; col = col->next)
    {
    if (col->mergedIx != -1)
        newFfArray[col->mergedIx] = oldFfArray[col->ix];
    }
return newFfArray;
}

static struct fieldedTable *groupColumns(struct facetedTable *facTab, struct fieldedTable *tableIn,
    struct facetField *selList, struct facetField ***retFfArray)
/* Create a new table that has somewhat fewer fields than the input table based on grouping
 * together columns based on isMerged values of selList.  Returns a new ffArray to go with new
 * table. */
{
struct mergeColHelper *countCol = NULL, *valCol = NULL, *colorCol = NULL;
struct mergeColHelper *colList = makeColList(facTab, tableIn, selList, 
    &countCol, &valCol, &colorCol);
struct mergeGroup *mergeList = findMergeGroups(tableIn, colList);
int newFieldCount;
struct mergeColHelper **outCols;
annotateColListAndMakeNewFields(colList, mergeList, &newFieldCount, &outCols);

/* Convert array of columns to array of names */
char *newFields[newFieldCount];
int i;
for (i=0; i<newFieldCount; ++i)
    newFields[i] = outCols[i]->name;

struct fieldedTable *tableOut = fieldedTableNew("merged", newFields, newFieldCount);
char *newRow[newFieldCount];
struct mergeGroup *merge;
for (merge = mergeList; merge != NULL; merge = merge->next)
    {
    char countBuf[32], valBuf[32], colorBuf[8];
    struct mergeColHelper *col;
    for (col = colList; col != NULL; col = col->next)
        {
	if (col->mergedIx != -1)
	    {
	    if (col->isKey)
	        {
		newRow[col->mergedIx] = merge->key;
		}
	    else if (col->isCount)
	        {
		long totalCount = summedCountAt(col->ix, merge);
		safef(countBuf, sizeof(countBuf), "%ld", totalCount);
		newRow[col->mergedIx] = countBuf;
		}
	    else if (col->isVal)
	        {
		double val;
		if (countCol == NULL)
		    val = aveValAt(col->ix, merge);
		else
		    val = weightedAveValAt(countCol->ix, col->ix, merge);
		safef(valBuf, sizeof(valBuf), "%g", val);
		newRow[col->mergedIx] = valBuf;
		}
	    else if (col->isColor)
	        {
		int r,g,b;
		weightedAveColorAt(countCol, col->ix, merge, &r, &g, &b);
		safef(colorBuf, sizeof(colorBuf), "#%02X%02X%02X", r, g, b);
		newRow[col->mergedIx] = colorBuf;
		}
	    else if (col->sameAfterMerger)
	        {
		newRow[col->mergedIx] = merge->aRow->row[col->ix];
		}
	    }
	}
    fieldedTableAdd(tableOut, newRow, newFieldCount, tableOut->rowCount);
    }

*retFfArray = recomputeFfArray(facTab->ffArray, colList, newFieldCount);
return tableOut;
}

struct fieldedTable *facetedTableSelect(struct facetedTable *facTab, struct cart *cart,
    struct facetField ***retFfArray)
/* Return table containing rows of table that have passed facet selection */
{
char *selList = facetedTableSelList(facTab, cart);
struct fieldedTable *subtable = facetFieldSelectRows(facTab->table, selList, facTab->ffArray);
struct facetField *ffSelList  = deLinearizeFacetValString(selList);
if (facTab->mergeFacetsOk)
    {
    return groupColumns(facTab, subtable, ffSelList, retFfArray);
    }
else
    {
    *retFfArray = facTab->ffArray;
    return subtable;
    }
}

struct slInt *facetedTableSelectOffsets(struct facetedTable *facTab, struct cart *cart)
/* Return a list of row positions that pass faceting */
{
char *selList = facetedTableSelList(facTab, cart);
struct fieldedTable *ft = facTab->table;
int fieldCount = ft->fieldCount;
struct slInt *retList = NULL;
facetFieldsFromSqlTableInit(ft->fields, fieldCount,
    selList, facTab->ffArray);
struct fieldedRow *fr;
int ix = 0;
for (fr = facTab->table->rowList; fr != NULL; fr = fr->next)
    {
    if (perRowFacetFields(fieldCount, fr->row, "", facTab->ffArray))
	{
	struct slInt *el = slIntNew(ix);
	slAddHead(&retList, el);
	}
    ++ix;
    }
slReverse(&retList);
return retList;
}

static int facetedTableMergedOffsetCmp(const void *va, const void *vb)
/* Compare to sort based on name start. */
{
const struct facetedTableMergedOffset *a = *((struct facetedTableMergedOffset **)va);
const struct facetedTableMergedOffset *b = *((struct facetedTableMergedOffset **)vb);
return cmpWordsWithEmbeddedNumbers(a->name, b->name);
}

struct facetedTableMergedOffset *facetedTableMakeMergedOffsets(struct facetedTable *facTab,
     struct cart *cart)
/* Return a structure that will let us relatively rapidly merge together one row */
{
struct fieldedTable *tableIn = facTab->table;
fieldedTableResetRowIds(tableIn, 0);	// Use the seldom-used ID column as indexes

/* Figure out user selection of fields to select on and merge as ffSelList */
char *selList = facetedTableSelList(facTab, cart);
struct facetField *ffSelList  = deLinearizeFacetValString(selList);

/* Remove columns that have not been selected */
struct fieldedTable *subtable = facetFieldSelectRows(facTab->table, selList, facTab->ffArray);
// struct fieldedTable *subtable = facTab->table; // ugly

/* Get output column list and pointers to key columns */
struct mergeColHelper *countCol = NULL, *valCol = NULL, *colorCol = NULL;
struct mergeColHelper *colList = makeColList(facTab, subtable, ffSelList, 
    &countCol, &valCol, &colorCol);
struct mergeGroup *mergeList = findMergeGroups(subtable, colList);
int newFieldCount;
struct mergeColHelper **outCol;
annotateColListAndMakeNewFields(colList, mergeList, &newFieldCount, &outCol);

struct facetedTableMergedOffset *tmoList = NULL;
struct mergeGroup *merge;

for (merge = mergeList; merge != NULL; merge = merge->next)
    {
    struct facetedTableMergedOffset *tmo;
    AllocVar(tmo);
    slAddHead(&tmoList, tmo);
    tmo->name = merge->key;
    int r,g,b;
    weightedAveColorAt(countCol, colorCol->ix, merge, &r, &g, &b);
    safef(tmo->color, sizeof(tmo->color), "#%02X%02X%02X", r, g, b);

    struct slRef *ref;
    for (ref = merge->rowRefList; ref != NULL; ref = ref->next)
        {
	struct fieldedRow *fr = ref->val;
	struct facetedTableCountOffset *tco;
	AllocVar(tco);
	tco->valIx = fr->id;
	int count = tco->count = (countCol != NULL ? sqlUnsigned(fr->row[countCol->ix]) : 1);
	tmo->totalCount += count;
	slAddHead(&tmo->colList, tco);
	}
    slReverse(&tmo->colList);
    }

/* Sort and attach ascending output indexes. */
slSort(&tmoList, facetedTableMergedOffsetCmp);
int outIx = 0;
struct facetedTableMergedOffset *tmo;
for (tmo = tmoList; tmo != NULL; tmo = tmo->next)
    {
    tmo->outIx = outIx++;
    }
return tmoList;
}

void facetedTableMergeVals(struct facetedTableMergedOffset *tmoList, float *inVals, int inValCount,
    float *outVals, int outValCount)
/* Populate outVals array with columns of weighted averages derived from applying
 * tmoList to inVals array. */
{
struct facetedTableMergedOffset *tmo;
for (tmo = tmoList; tmo != NULL; tmo = tmo->next)
    {
    if (tmo->outIx < 0 || tmo->outIx >= outValCount) errAbort("facetTableMergeVals finds outIx out of range %d not in [0-%d)\n", tmo->outIx, outValCount);
    struct facetedTableCountOffset *colList = tmo->colList;
    if (colList->next == NULL)  // specail case of one
        {
	outVals[tmo->outIx] = inVals[colList->valIx];
	}
    else
        {
	double scale = 1.0/tmo->totalCount;
	double weightedSum = 0;
	struct facetedTableCountOffset *col;
	for (col = colList; col != NULL; col = col->next)
	    {
	    weightedSum += inVals[col->valIx] * (double)col->count;
	    }
	outVals[tmo->outIx] = weightedSum*scale;
	}
    }
}


void facetedTableWriteHtml(struct facetedTable *facTab, struct cart *cart,
    struct fieldedTable *selected, struct facetField **selectedFf, char *displayList, 
    char *returnUrl, int maxLenField, 
    struct hash *tagOutputWrappers, void *wrapperContext, int facetUsualSize)
/* Write out the main HTML associated with facet selection and table. */
{
facetedTableWebInit();
struct hash *emptyHash = hashNew(0);
webFilteredFieldedTable(cart, selected, 
    displayList, returnUrl, facTab->varPrefix, 
    maxLenField, tagOutputWrappers, wrapperContext,
    FALSE, NULL,
    selected->rowCount, facetUsualSize,
    NULL, emptyHash,
    selectedFf, facTab->facets,
    NULL, facTab->mergeFacetsOk);
hashFree(&emptyHash);
}

