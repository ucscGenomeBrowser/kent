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
     struct slRef *valCursor; /* Where we are in valList */
     boolean captured;	/* Used internally when finding best partitioning */
     boolean chosen;	/* Used internally to mark best partition */
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

boolean fieldsAreLocked(struct fieldedTable *table, struct fieldInfo *a, struct fieldInfo *b)
/* Return TRUE if fields are locked in the sense that you can predict the value of one field
 * from the value of the other. */
{
/* A field is always locked to itself! */
if (a == b)
    return TRUE;

/* If they don't have same number of values they can't be locked */
if (a->valHash->elCount != b->valHash->elCount)
    return FALSE;

/* Ok, have to do hard work of checking values move in step.  Do this with aid of
 * hash keyed by a with values from b. */
boolean areLocked = TRUE;
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
	    areLocked = FALSE;
	    break;
	    }
	}
    else
        {
	hashAdd(hash, aVal, bVal);
	}
    }
hashFree(&hash);
return areLocked;
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

struct slRef *findLockedFields(struct fieldedTable *table, struct fieldInfo *fieldList,
    struct fieldInfo *primaryField, struct slName *exceptList)
/* Return list of fields from fieldList that move in lock-step with primary field */
{
struct slRef *list = NULL;
struct fieldInfo *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    if (slNameFind(exceptList, field->name) == NULL)
	if (field->captured == FALSE && fieldsAreLocked(table, primaryField, field))
	    refAdd(&list, field);
    }
slReverse(&list);
return list;
}

boolean findBestParting(struct fieldedTable *table, struct fieldInfo *allFields,
    struct slRef **retFields)
/* Return list of fields that make best partition of table so that it will
 * cause the most information from table to be moved to a higher level.  All the
 * returned fields will move in lock-step. */
{

/* Find count of number of values in field with fewest values */
int smallestValCount = BIGNUM;
struct fieldInfo *field;
for (field = allFields; field != NULL; field = field->next)
    {
    int valCount = field->valHash->elCount;
    if (valCount < smallestValCount)
         smallestValCount = valCount;
    }
if (smallestValCount >= slCount(table->rowList)/2)
    {
    retFields = NULL;
    return FALSE;
    }

/* Of the ones that have the smallest number of values, find the set with the
 * most fields locked together */
int mostLocked = 0;
struct slRef *mostLockedList = NULL;
for (field = allFields; field != NULL; field = field->next)
    {
    if (field->valHash->elCount == smallestValCount && !field->captured)
        {
	/* Get list of fields locked to this one, list includes self */
	struct slRef *lockedFields = findLockedFields(table, field, field, NULL);

	/* Mark these fields as captured */
	struct slRef *ref;
	for (ref = lockedFields; ref != NULL; ref = ref->next)
	    {
	    struct fieldInfo *info = ref->val;
	    info->captured = TRUE;
	    }

	/* See if we've got more fields than before, and if so update best one. */
	int lockedCount = slCount(lockedFields);
	if (lockedCount > mostLocked)
	    {
	    slFreeList(&mostLockedList);
	    mostLockedList = lockedFields;
	    mostLocked = lockedCount;
	    }
	}
    }

/* Mark the fields in the best partition as chosen */
struct slRef *ref;
for (ref = mostLockedList; ref != NULL; ref = ref->next)
    {
    struct fieldInfo *info = ref->val;
    info->chosen = TRUE;
    }

/* Move chosen fields to return list, and free the rest */
struct slRef *retList = NULL;
for (field = allFields; field != NULL; field = field->next)
    {
    if (field->chosen)
	refAdd(&retList, field);
    }
slReverse(&retList);
*retFields = retList;
return TRUE;
}

struct slRef *partOnField(struct fieldedTable *table, struct fieldInfo *allFields,
    char *keyName, struct slName *except)
/* Return list of all fields that covary with keyField */
{
struct fieldInfo *keyField = fieldInfoFind(allFields, keyName);
if (keyField == NULL)
    errAbort("%s not found in fields", keyName);
return findLockedFields(table, allFields, keyField, except);
}


boolean doParting(struct fieldedTable *table, struct fieldInfo *allFields,
    boolean doPrepart, struct slName *preparting, struct slRef **retFields)
/* Partition table factoring out all fields that move in lockstep at this level
 * or all remaining fields if down to lowest level. */
{
if (doPrepart)
    {
    if (preparting == NULL || preparting->next == NULL)
        return FALSE;
    *retFields = partOnField(table, allFields, preparting->name, preparting->next);
    return TRUE;
    }
else
    return findBestParting(table, allFields, retFields);
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

void makeSubtableExcluding(struct fieldedTable *table, struct slRef *excludedFields,
    struct fieldedTable **retSubtable, int ixTranslator[])
/* Make up a subtable that contains all the fields of the table except the excluded ones */
{
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
*retSubtable = subtable;
}

void rPartition(struct fieldedTable *table, struct tagStorm *tagStorm, struct tagStanza *parent,
    boolean doPrepart, struct slName *prepartField)
/* Recursively partition table and add to tagStorm. */
{
struct fieldInfo *allFields = makeFieldInfo(table);
struct slRef *partingFields = NULL;
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
if (!doParting(table, allFields, doPrepart, prepartField, &partingFields))
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

/* Position field cursors within partingFields */
struct slRef *ref;
for (ref = partingFields; ref != NULL; ref = ref->next)
    {
    struct fieldInfo *field = ref->val;
    field->valCursor = field->valList;
    }

struct fieldInfo *partingField = partingFields->val;
int partingFieldIx = partingField->ix;

/* Since the parting values are sorted by where they appear in table, we can
 * just scan through the table once.  We'll output the values at the first
 * place they appear. */
struct slRef *partVal;
for (partVal = partingField->valList; partVal != NULL; partVal = partVal->next)
    {
    /* Add constant bits to tagStorm */
    struct tagStanza *stanza = tagStanzaNew(tagStorm, parent);
    for (ref = partingFields; ref != NULL; ref = ref->next)
        {
	struct fieldInfo *field = ref->val;
	char *val = field->valCursor->val;
	field->valCursor = field->valCursor->next;
	tagStanzaAdd(tagStorm, stanza, field->name, val);
	}
    slReverse(&stanza->tagList);

    /* Make subtable with nonconstant bits starting with header. */
    char *val = partVal->val;
    int ixTranslator[table->fieldCount];
    struct fieldedTable *subtable = NULL;
    makeSubtableExcluding(table, partingFields, &subtable, ixTranslator);

    /* Loop through old table saving parts from nonconstant tags in subtable */
    int subcount = subtable->fieldCount;
    char *subrow[subcount];
    struct fieldedRow *row = table->rowList;
    for (row = table->rowList; row != NULL; row = row->next)
        {
	if (sameString(row->row[partingFieldIx], val))
	    {
	    int i;
	    for (i=0; i<table->fieldCount; ++i)
	        {
		int subi = ixTranslator[i];
		if (subi >= 0)
		    subrow[subi] = row->row[i];
		}
	    fieldedTableAdd(subtable, subrow, subcount, row->id);
	    }
	}


    rPartition(subtable, tagStorm, stanza, doPrepart, nextPrepart);
    }
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
