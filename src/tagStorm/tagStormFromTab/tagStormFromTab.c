/* tagStormFromTab - Create a tagStorm file from a tab-separated file where the labels are on the 
 * first line, that starts with a #. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "tagStorm.h"

struct slName *divFieldList = NULL;  // Filled in from div command line option.

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormFromTab - Create a tagStorm file from a tab-separated file where the labels are on the\n"
  "first line, that starts with a #.\n"
  "usage:\n"
  "   tagStormFromTab in.tab out.tags\n"
  "options:\n"
  "   -div=fields,to,divide,on - comma separated list of fields, from highest to lowest level\n"
  "                              to partition data on. Otherwise will be calculated.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"div", OPTION_STRING},
   {NULL, 0},
};

struct fieldInfo
/* Information about a field in table. */
     {
     struct fieldInfo *next;
     char *name;    /* Field name */
     int ix;	    /* Field position */
     struct hash *valHash;   /* Each unique value */
     struct slRef *valList;  /* String valued list of possible values for this field */
     };

void fieldInfoFree(struct fieldInfo **pField)
/* Free information associated with a fieldInfo */
{
struct fieldInfo *field = *pField;
if (field != NULL)
    {
    slFreeList(&field->valList);
    hashFree(&field->valHash);
    freez(pField);
    }
}

void fieldInfoFreeList(struct fieldInfo **pList)
/* Free a list of dynamically allocated fieldInfo's */
{
struct fieldInfo *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    fieldInfoFree(&el);
    }
*pList = NULL;
}


struct fieldInfo *fieldInfoFind(struct fieldInfo *list, char *name)
/* Find element of given name in list. */
{
struct fieldInfo *fi;
for (fi = list; fi != NULL; fi = fi->next)
    {
    if (sameString(name, fi->name))
        return fi;
    }
return NULL;
}

boolean aPredictsB(struct fieldedTable *table, struct fieldInfo *a, struct fieldInfo *b)
/* Return TRUE if fields are locked in the sense that you can predict the value of b from the value of a */
{
/* A field is always locked to itself! */
if (a == b)
    return TRUE;

/* If a doesnt have as many vales as b, no way can b be locked to a */
if (a->valHash->elCount < b->valHash->elCount)
    return FALSE;

/* Ok, have to do hard work of checking values move in step.  Do this with aid of
 * hash keyed by a with values from b. */
boolean arePredictable = TRUE;
struct hash *hash = hashNew(0);
int aIx = a->ix, bIx = b->ix;
struct fieldedRow *row;
for (row = table->rowList; row != NULL; row = row->next)
    {
    char *aVal = row->row[aIx];
    char *bVal = row->row[bIx];
    char *bOldVal = hashFindVal(hash, aVal);
    if (bOldVal != NULL)
	{
	if (!sameString(bVal, bOldVal))
	    {
	    arePredictable = FALSE;
	    break;
	    }
	}
    else
        {
	hashAdd(hash, aVal, bVal);
	}
    }
hashFree(&hash);
return arePredictable;
}

struct fieldInfo *makeFieldInfo(struct fieldedTable *table)
/* Make list of field info for table */
{
struct fieldInfo *fieldList = NULL, *field;
int fieldIx;
for (fieldIx=0; fieldIx < table->fieldCount; ++fieldIx)
    {
    AllocVar(field);
    field->name = table->fields[fieldIx];
    field->ix = fieldIx;
    struct hash *hash = field->valHash = hashNew(0);
    struct fieldedRow *row;
    for (row = table->rowList; row != NULL; row = row->next)
        {
	char *val = row->row[fieldIx];
	if (!hashLookup(hash, val))
	    {
	    refAdd(&field->valList, val);
	    hashAdd(hash, val, NULL);
	    }
	}
    slReverse(&field->valList);
    slAddHead(&fieldList, field);
    }
slReverse(&fieldList);
return fieldList;
}

struct slRef *findPredictableFields(struct fieldedTable *table, struct fieldInfo *fieldList,
    struct fieldInfo *primaryField, struct slName *exceptList)
/* Return list of fields from fieldList that move in lock-step with primary field */
{
struct slRef *list = NULL;
struct fieldInfo *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    if (slNameFind(exceptList, field->name) == NULL)
	if (aPredictsB(table, primaryField, field))
	    refAdd(&list, field);
    }
slReverse(&list);
return list;
}

boolean findBestParting(struct fieldedTable *table, struct fieldInfo *allFields,
    struct slRef **retFields, struct fieldInfo **retField)
/* Return list of fields that make best partition of table so that it will
 * cause the most information from table to be moved to a higher level.  All the
 * returned fields will move in lock-step. */
{
double bestLinesSaved = 0;
struct slRef *partingList = NULL;
struct fieldInfo *partingField = NULL;
int tableRows = slCount(table->rowList);
int oldLineCount = tableRows * (table->fieldCount + 1);
verbose(2, "Find best parting on table with %d rows and %d fields\n", tableRows, table->fieldCount);

/* Of the ones that have the smallest number of values, find the set with the
 * most fields locked together */
struct fieldInfo *field;
for (field = allFields; field != NULL; field = field->next)
    {
    /* Get list of fields locked to this one, list includes self */
    struct slRef *lockedFields = findPredictableFields(table, allFields, field, NULL);

    /* We do some weighting to reduce the value of fields which are predictable but move slower 
     * than we do. Indirectly this lets these form a higher level structure on top of us. */
    double fieldValCount = field->valHash->elCount;
    double q = 1.0/fieldValCount; 
    double lockWeight = 0;
    struct slRef *ref;
    for (ref = lockedFields; ref != NULL; ref = ref->next)
        {
        struct fieldInfo *fi = ref->val;
	double ratio = fi->valHash->elCount * q;
	if (ratio > 1)
	    ratio = 1;
	lockWeight += ratio;
	}

    double varyCount = table->fieldCount - lockWeight;
    double newLineCount = field->valHash->elCount * (1 + lockWeight) + tableRows * (1 + varyCount);
    double linesSaved = oldLineCount - newLineCount;
    verbose(2, "field %s, %d vals, %g locked, %g lines saved\n", field->name, 
	field->valHash->elCount, lockWeight, linesSaved);
    if (linesSaved > bestLinesSaved)
	{
	bestLinesSaved = linesSaved;
	slFreeList(&partingList);
	partingList = lockedFields;
	partingField = field;
	lockedFields = NULL;
	}
    else
	slFreeList(&lockedFields);
    }
*retFields = partingList;
*retField = partingField;
return partingList != NULL;
}

struct slRef *partOnField(struct fieldedTable *table, struct fieldInfo *allFields,
    char *keyName, struct slName *except, struct fieldInfo **retKeyField)
/* Return list of all fields that covary with keyField */
{
struct fieldInfo *keyField = fieldInfoFind(allFields, keyName);
if (keyField == NULL)
    errAbort("%s not found in fields", keyName);
*retKeyField = keyField;
return findPredictableFields(table, allFields, keyField, except);
}


boolean doParting(struct fieldedTable *table, struct fieldInfo *allFields,
    boolean doPrepart, struct slName *preparting, struct slRef **retFields,
    struct fieldInfo **retField)
/* Partition table factoring out fields that move together.  
 * Return TRUE  and leave a list of references to fieldInfo's in *retField if can find
 * a useful partitioning, otherwise return FALSE. 
 *    The retFields returns a list of references to all fields to factor out in the partitioning.
 *    The retField contains the parting field itself (which is also in retFields) */
{
if (doPrepart)
    {
    if (preparting == NULL || preparting->next == NULL)
        return FALSE;
    *retFields = partOnField(table, allFields, preparting->name, preparting->next, retField);
    return TRUE;
    }
else
    return findBestParting(table, allFields, retFields, retField);
}

static boolean inFieldRefList(struct slRef *fieldRefList, char *name)
/* Look for named field in fieldRefList */
{
struct slRef *ref;
for (ref = fieldRefList; ref != NULL; ref = ref->next)
    {
    struct fieldInfo *field = ref->val;
    if (sameString(field->name, name))
         return TRUE;
    }
return FALSE;
}

void makeSubtableExcluding(struct fieldedTable *table, 
    int partingFieldIx, char *partingVal,
    struct slRef *excludedFields,
    struct fieldedTable **retSubtable, int ixTranslator[])
/* Make up a subtable that contains all the fields of the table except the excluded ones */
{
/* Make new subtable with correct fields */
int excludedCount = slCount(excludedFields);
int subFieldCount = table->fieldCount - excludedCount;
assert(subFieldCount > 0);
char *subFields[subFieldCount];
int i, subI=0;
for (i=0; i<table->fieldCount; ++i)
    {
    char *field = table->fields[i];
    ixTranslator[i] = -1;
    if (!inFieldRefList(excludedFields, field))
        {
	subFields[subI] = field;
	ixTranslator[i] = subI;
	++subI;
	}
    }
assert(subI == subFieldCount);
struct fieldedTable *subtable = fieldedTableNew("subtable", subFields, subFieldCount);

/* Populate rows of subtable with subsets of rows of original table where the partingField
 * matches the parting value */
char *subrow[subFieldCount];
struct fieldedRow *row;
for (row = table->rowList; row != NULL; row = row->next)
    {
    if (sameString(row->row[partingFieldIx], partingVal))
	{
	int i;
	for (i=0; i<table->fieldCount; ++i)
	    {
	    int subi = ixTranslator[i];
	    if (subi >= 0)
		subrow[subi] = row->row[i];
	    }
	fieldedTableAdd(subtable, subrow, subFieldCount, row->id);
	}
    }

*retSubtable = subtable;
}

void rPartition(struct fieldedTable *table, struct tagStorm *tagStorm, struct tagStanza *parent,
    boolean doPrepart, struct slName *prepartField)
/* Recursively partition table and add to tagStorm. */
{
struct fieldInfo *allFields = makeFieldInfo(table);
struct slName *nextPrepart = NULL;
if (doPrepart)
    {
    if (prepartField != NULL)
	nextPrepart = prepartField->next;
    }
else
    {
    prepartField = NULL;
    }
struct slRef *partingFields = NULL;
struct fieldInfo *partingField = NULL;
if (!doParting(table, allFields, doPrepart, prepartField, &partingFields, &partingField))
    {
    // Here is where we should output whole table... 
    struct fieldedRow *row;
    for (row = table->rowList; row != NULL; row = row->next)
        {
	struct tagStanza *stanza = tagStanzaNew(tagStorm, parent);
	int i;
	for (i=0; i<table->fieldCount; ++i)
	    tagStanzaAdd(tagStorm, stanza, table->fields[i], row->row[i]);
	}
    return;
    }

int partingFieldIx = partingField->ix;


/* Scan through table once outputting constant bits into a tag-storm */
struct hash *uniq = hashNew(0);
struct slRef *partValList = partingField->valList;
char *partVal = NULL;
struct fieldedRow *fieldedRow;
for (fieldedRow = table->rowList; fieldedRow != NULL; fieldedRow = fieldedRow->next)
    {
    char **row = fieldedRow->row;
    char *keyVal = row[partingFieldIx];
    if (partVal == NULL || differentString(partVal, keyVal))
        {
	if (hashLookup(uniq, keyVal) == NULL)
	    {
	    hashAdd(uniq, keyVal, NULL);

	    /* Add current values of parting field to stanza */
	    struct tagStanza *stanza = tagStanzaNew(tagStorm, parent);
	    struct slRef *ref;
	    for (ref = partingFields; ref != NULL; ref = ref->next)
		{
		struct fieldInfo *field = ref->val;
		tagStanzaAdd(tagStorm, stanza, field->name, row[field->ix]);
		}
	    slReverse(&stanza->tagList);

	    /* Advance to next value */
	    assert(partValList != NULL);
	    partVal = partValList->val;
	    partValList = partValList->next;

	    /* Make subtable with nonconstant bits starting with header. */
	    verbose(2, "parting on %s=%s\n", partingField->name, partVal);
	    int ixTranslator[table->fieldCount];
	    struct fieldedTable *subtable = NULL;
	    makeSubtableExcluding(table, partingFieldIx, partVal,
		partingFields, &subtable, ixTranslator);

	    rPartition(subtable, tagStorm, stanza, doPrepart, nextPrepart);
	    }
	}

    }
hashFree(&uniq);

slFreeList(&partingFields);
fieldInfoFreeList(&allFields);
}

void tagStormFromTab(char *input, char *output)
/* tagStormFromTab - Create a tagStorm file from a tab-separated file where the labels are on the 
 * first line, that starts with a #. */
{
struct fieldedTable *table = fieldedTableFromTabFile(input, input, NULL, 0);
struct tagStorm *tagStorm = tagStormNew(input);
rPartition(table, tagStorm, NULL, divFieldList != NULL, divFieldList);
tagStormReverseAll(tagStorm);
tagStormWrite(tagStorm, output, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

// Process command line option to drive the partitioning manually.
char *div = optionVal("div", NULL);
if (div != NULL)
    divFieldList = slNameListFromComma(div);

tagStormFromTab(argv[1], argv[2]);
return 0;
}
