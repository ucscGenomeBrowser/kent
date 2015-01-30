/* tagStormFromTab - Create a tagStorm file from a tab-separated file where the labels are on the 
 * first line, that starts with a #. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormFromTab - Create a tagStorm file from a tab-separated file where the labels are on the\n"
  "first line, that starts with a #.\n"
  "usage:\n"
  "   tagStormFromTab in.tab out.tags\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
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
    struct fieldInfo *primaryField)
/* Return list of fields from fieldList that move in lock-step with primary field */
{
struct slRef *list = NULL;
struct fieldInfo *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    if (field->captured == FALSE && fieldsAreLocked(table, primaryField, field))
        refAdd(&list, field);
    }
slReverse(&list);
return list;
}

boolean findBestParting(struct fieldedTable *table, struct fieldInfo **retFields)
/* Return list of fields that make best partition of table so that it will
 * cause the most information from table to be moved to a higher level.  All the
 * returned fields will move in lock-step. */
{
struct fieldInfo *allFields = makeFieldInfo(table), *field;

/* Find count of number of values in field with fewest values */
int smallestValCount = BIGNUM;
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
	struct slRef *lockedFields = findLockedFields(table, field, field);

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
struct fieldInfo *retList = NULL, *next;
for (field = allFields; field != NULL; field = next)
    {
    next = field->next;
    if (field->chosen)
        slAddHead(&retList, field);
    else
        fieldInfoFree(&field);
    }
slReverse(&retList);
*retFields = retList;
return TRUE;
}


void rPartition(struct fieldedTable *table, struct tagStorm *tagStorm, struct tagStanza *parent)
/* Recursively partition table and add to tagStorm. */
{
struct fieldInfo *partingFields = NULL;
if (!findBestParting(table, &partingFields))
    return;

int firstFieldIx = partingFields->ix;

/* Since the parting values are sorted by where they appear in table, we can
 * just scan through the table once.  We'll output the values at the first
 * place they appear. */
struct fieldedRow *row = table->rowList;
struct slRef *partVal;
for (partVal = partingFields->valList; partVal != NULL; partVal = partVal->next)
    {
    struct tagStanza *stanza = tagStanzaNew(tagStorm, parent);
    char *val = partVal->val;
    for (;;)
        {
	if (row == NULL)
	    {
	    warn("partingValues not sorted");
	    internalErr();
	    }
	if (sameString(row->row[firstFieldIx], val))
	    {
	    struct fieldInfo *field;
	    for (field = partingFields; field != NULL; field = field->next)
		tagStanzaAdd(tagStorm, stanza, field->name, row->row[field->ix]);
	    break;
	    }
	row = row->next;
	}
    }

fieldInfoFreeList(&partingFields);
}

void tagStormFromTab(char *input, char *output)
/* tagStormFromTab - Create a tagStorm file from a tab-separated file where the labels are on the 
 * first line, that starts with a #. */
{
struct fieldedTable *table = fieldedTableFromTabFile(input, input, NULL, 0);
uglyf("Got %d fields in %s\n", table->fieldCount, input);
struct tagStorm *tagStorm = tagStormNew(input);
rPartition(table, tagStorm, NULL);
tagStormReverseAll(tagStorm);
tagStormWrite(tagStorm, output, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tagStormFromTab(argv[1], argv[2]);
return 0;
}
