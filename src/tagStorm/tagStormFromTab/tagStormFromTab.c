/* tagStormFromTab - Create a tagStorm file from a tab-separated file where the labels are on the 
 * first line, that starts with a #. 
 *
 * The basic idea of this is
 * 1) Read into into a fieldedTable structure.
 * 2) Figure out semi-magically what field to partition on.
 * 3) For each value of that field, write out a stanza that includes all fields that are 
 *    constant for that value of the partitioning field.
 * 4) Create a new, smaller fieldedTable that just consists of the remaining fields, 
 *    and the rows corresponding to the stanza.
 * 5) Recurse to 2. 
 *
 * The field to partition on in step 2 is either specified by the command line with the
 * -div= option, or calculated in findLockedSets() where partingScore is set. The current
 * partingScore algorithm (the third version) prefers fields with relatively few values,
 * ideally non-numerical,  with lots of fields that move in sync with it, and where it leaves
 * open further parting by other fields.  This further partability is calculated in the
 * longestPredictorChainSize() function. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "tagStorm.h"
#include "tagToSql.h"

struct slName *gDivFieldList = NULL;  // Filled in from div command line option.

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormFromTab - Create a tagStorm file from a tab-separated file where the labels are on the\n"
  "first line, that may start with a #.\n"
  "usage:\n"
  "   tagStormFromTab in.tab out.tags\n"
  "options:\n"
  "   -div=fields,to,divide,on - comma separated list of fields, from highest to lowest level\n"
  "                              to partition data on. Otherwise will be calculated.\n"
  "   -local - calculate fields to divide on locally and recursively rather than globally.\n"
  "   -noHoist - don't automatically move tags to a higher level when possible.\n"
  "   -keepOrder - keep field order from input file rather than alphabetizing\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"div", OPTION_STRING},
   {"local", OPTION_BOOLEAN},
   {"noHoist", OPTION_BOOLEAN},
   {"keepOrder", OPTION_BOOLEAN},
   {NULL, 0},
};

struct fieldInfo
/* Information about a field in table. */
     {
     struct fieldInfo *next;
     char *name;    /* Field name. Not allocated here. */
     int ix;	    /* Field position */
     struct hash *valHash;   /* Each unique value */
     struct slRef *valList;  /* String valued list of possible values for this field */
     int realValCount;	    /* Number of non-NULL values */
     struct tagTypeInfo *typeInfo;  /* Information about type and range of values */
     };

struct lockedSet
/* This represents a group of fields that always move together */
    {
    struct lockedSet *next;
    char *name;	    /* Taken from name of first field. Not allocated here. */
    int ix;	    /* Position of first field in table */
    struct fieldInfo *firstField;  /* First field in locked set. Not allocated here. */
    int valCount;   /* Number of distinct values */
    double realValRatio;  /* Proportion of non-NULL values */
    struct slRef *fieldRefList;  /* Field info valued */
    int predictorCount;   /* Number of lockedSets that can predict us */
    struct slRef *predictorList;  /* List of lockedSets we predict */
    int predictedCount;   /* Number of lockedSets that we predict */
    struct slRef *predictedList;  /* List of lockedSets we predict */
    int predictorChainSize;	/* Size of longest chain of predictors */
    double partingScore;  /* How ugood this looks as a partitioner */
    boolean allFloatingPoint;  /* True if all fields are floating point */
    boolean allInt;	    /* True if all fields are int */
    };

static void fieldInfoFree(struct fieldInfo **pField)
/* Free up memory associated with fieldInfo */
{
struct fieldInfo *field = *pField;
if (field != NULL)
    {
    hashFree(&field->valHash);
    slFreeList(&field->valList);
    tagTypeInfoFree(&field->typeInfo);
    freez(pField);
    }
}

static void fieldInfoFreeList(struct fieldInfo **pList)
/* Free a list of dynamically allocated fieldInfo */
{
struct fieldInfo *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    fieldInfoFree(&el);
    }
*pList = NULL;
}

static void lockedSetFree(struct lockedSet **pSet)
/* Free up memory associated with a locked set */
{
struct lockedSet *set = *pSet;
if (set != NULL)
    {
    slFreeList(&set->fieldRefList);
    slFreeList(&set->predictedList);
    slFreeList(&set->predictorList);
    freez(pSet);
    }
}

static void lockedSetFreeList(struct lockedSet **pList)
/* Free a list of dynamically allocated locked sets */
{
struct lockedSet *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    lockedSetFree(&el);
    }
*pList = NULL;
}


static boolean isDefinedVal(char *s)
/* Return TRUE if it looks like s is a real value, that is non-NULL, non-empty, and
 * neither n/a nor N/A */
{
if (s == NULL) return FALSE;
if (s[0] == 0) return FALSE;
if (sameString(s, "n/a") || sameString(s, "N/A"))
    return FALSE;
return TRUE;
}

static struct fieldInfo *fieldInfoFind(struct fieldInfo *list, char *name)
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

static boolean aPredictsB(struct fieldedTable *table, struct fieldInfo *a, struct fieldInfo *b)
/* Return TRUE if fields are locked in the sense that you can predict the value of b from the value of a */
{
/* A field is always locked to itself! */
if (a == b)
    return TRUE;

/* If a doesnt have as many vales as b, no way can a predict b */
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

static struct fieldInfo *makeFieldInfo(struct fieldedTable *table)
/* Make list of field info for table */
{
struct fieldInfo *fieldList = NULL, *field;
int fieldIx;
for (fieldIx=0; fieldIx < table->fieldCount; ++fieldIx)
    {
    AllocVar(field);
    field->name = table->fields[fieldIx];
    field->ix = fieldIx;
    field->typeInfo = tagTypeInfoNew(field->name);
    struct hash *hash = field->valHash = hashNew(0);
    struct fieldedRow *row;
    for (row = table->rowList; row != NULL; row = row->next)
        {
	char *val = row->row[fieldIx];
	boolean isDef = isDefinedVal(val);
	if (isDef)
	    ++field->realValCount;
	if (!hashLookup(hash, val))
	    {
	    if (isDef) 
		tagTypeInfoAdd(field->typeInfo, val);
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

static bool **makePredMatrix(struct fieldedTable *table, struct fieldInfo *fieldList)
/* Make up matrix of aPredictsB results to avoid a relatively expensive recalculation */
{
int fieldCount = table->fieldCount;
bool **matrix;
AllocArray(matrix, fieldCount);
struct fieldInfo *aField, *bField;
for (aField = fieldList; aField != NULL; aField = aField->next)
    {
    int aIx = aField->ix;
    bool *row;
    AllocArray(row, fieldCount);
    matrix[aIx] = row;
    for (bField = fieldList; bField != NULL; bField = bField->next)
        {
	int bIx = bField->ix;
	row[bIx] = aPredictsB(table, aField, bField);
	}
    }
verbose(3, "made predMatrix of %d cells\n", fieldCount * fieldCount);
return matrix;
}

static void freePredMatrix(struct fieldedTable *table, bool ***pMatrix)
/* Free matrix make with makePredMatrix */
{
bool **matrix = *pMatrix;
if (pMatrix != NULL)
    {
    int fieldCount = table->fieldCount;
    int i;
    for (i=0; i<fieldCount; ++i)
        freeMem(matrix[i]);
    freez(pMatrix);
    }
}

static int lockedSetCmpPartingScore(const void *va, const void *vb)
/* Compare two lockedSets based on partingScore, descending. */
{
const struct lockedSet *a = *((struct lockedSet **)va);
const struct lockedSet *b = *((struct lockedSet **)vb);
double diff = a->partingScore - b->partingScore;
if (diff < 0)
    return 1;
else if (diff > 0)
    return -1;
else
    return 0;
}


static int longestPredictorChainSize(struct lockedSet *set)
/* Recursively calculate longest chain of predictors starting with set */
{
struct slRef *ref;
int longest = 0;
for (ref = set->predictorList; ref != NULL; ref = ref->next)
    {
    struct lockedSet *nextSet = ref->val;
    if (nextSet != set)
	{
	int size = longestPredictorChainSize(nextSet);
	if (size > longest)
	    longest = size;
	}
    }
return longest + 1;
}

static struct lockedSet *findLockedSets(struct fieldedTable *table, struct fieldInfo *fieldList)
/* Find locked together fields */
{
bool **predMatrix = makePredMatrix(table, fieldList);
struct lockedSet *setList = NULL;
struct hash *usedHash = hashNew(0);
struct fieldInfo *aField, *bField;
double rowCount = table->rowCount;
for (aField = fieldList; aField != NULL; aField = aField->next)
    {
    if (!hashLookup(usedHash, aField->name))
        {
	int aIx = aField->ix;
	struct lockedSet *set;
	AllocVar(set);
	set->name = aField->name;
	set->ix = aField->ix;
	set->firstField = aField;
	set->valCount = aField->valHash->elCount;
	set->realValRatio = aField->realValCount / rowCount;
	set->fieldRefList = slRefNew(aField);
	slAddHead(&setList, set);
	for (bField = aField->next; bField != NULL; bField = bField->next)
	    {
	    if (!hashLookup(usedHash, bField->name))
	        {
		int bIx = bField->ix;
		if (predMatrix[aIx][bIx] && predMatrix[bIx][aIx])
		    {
		    hashAdd(usedHash, bField->name, NULL);
		    refAdd(&set->fieldRefList, bField);
		    }
		}
	    }
	slReverse(&set->fieldRefList);
	}
    }
hashFree(&usedHash);

/* Fill in fields that can predict */
struct lockedSet *aSet;
for (aSet = setList; aSet != NULL; aSet = aSet->next)
    {
    int aIx = aSet->ix;
    struct lockedSet *bSet;
    for (bSet = setList; bSet != NULL; bSet = bSet->next)
        {
	int bIx = bSet->ix;
	if (predMatrix[bIx][aIx])
	    {
	    ++aSet->predictorCount;
	    refAdd(&aSet->predictorList, bSet);
	    }
	if (predMatrix[aIx][bIx])
	    {
	    ++aSet->predictedCount;
	    refAdd(&aSet->predictedList, bSet);
	    }
	}
    }

/* Figure out whether all types are numerical/integer */
for (aSet = setList; aSet != NULL; aSet = aSet->next)
    {
    boolean allInt = TRUE, allFloatingPoint = TRUE;
    struct slRef *ref;
    for (ref = aSet->fieldRefList; ref != NULL; ref = ref->next)
        {
	struct fieldInfo *field = ref->val;
	struct tagTypeInfo *tti = field->typeInfo;
	verbose(3, "%s isInt %d, isNum %d\n", field->name, tti->isInt, tti->isNum);
	if (!tti->isInt)
	   allInt = FALSE;
	if (!tti->isNum)
	   allFloatingPoint = FALSE;
	}
    aSet->allInt = allInt;
    aSet->allFloatingPoint = allFloatingPoint;
    verbose(3, "tot: %s allInt %d, allFloatingPoint %d\n", aSet->name, aSet->allInt, aSet->allFloatingPoint);
    }

/* Calculate score and sort on it. */
for (aSet = setList; aSet != NULL; aSet = aSet->next)
    {
    int lockedCount = slCount(aSet->fieldRefList);
    double real = aSet->realValRatio;
    aSet->predictorChainSize = longestPredictorChainSize(aSet);
    double partingScore = 4*lockedCount;	// Locked fields are really good!
    partingScore += 2*aSet->predictorCount;	// Lots of predictors means we are high level
    partingScore += aSet->predictedCount;	// Predicted things are good too
    partingScore *= aSet->predictorChainSize;   // Long predictor chains mean high level
    partingScore *= real*real;			// Unreal things are bad-bad
    partingScore /= pow(aSet->valCount, 0.5);	// Lots of values not good
    if (aSet->allFloatingPoint)
        {
	if (aSet->allInt)
	    partingScore *= 0.5;		// Integers are not great to partition on
	else
	    partingScore *= 0.1;		// But floating point numbers are even worse
	}
    aSet->partingScore = partingScore;
    }
slSort(&setList, lockedSetCmpPartingScore);

/* Clean up and go home. */
freePredMatrix(table, &predMatrix);
return setList;
}

static void dumpLockedSetList(struct lockedSet *lockedSetList, int verbosity)
/* Print out info on locked sets to file at a given verbosity level */
{
struct lockedSet *set;
for (set = lockedSetList; set != NULL; set = set->next)
     {
     verbose(verbosity,	    
        "%s: %d vals, %g real, %d locked, %d pred me, %d iPred, %d pChain, %s, %s, %g score\n",
	set->name, set->valCount, set->realValRatio, slCount(set->fieldRefList), 
	set->predictedCount, set->predictorCount, set->predictorChainSize, 
	(set->allFloatingPoint ? "num" : "!num"),
	(set->allInt ? "int" : "!int"),
	set->partingScore);
     struct slRef *ref;
     verbose(verbosity, "\tlocked:");
     for (ref = set->fieldRefList; ref != NULL; ref = ref->next)
         {
	 struct fieldInfo *field = ref->val;
	 if (field != set->firstField)
	     verbose(verbosity, " %s", field->name);
	 }
     verbose(verbosity, "\n");
     verbose(verbosity, "\tpredicted:");
     for (ref = set->predictedList; ref != NULL; ref = ref->next)
         {
	 struct lockedSet *ls = ref->val;
	 if (ls != set)
	     verbose(verbosity, " %s", ls->name);
	 }
     verbose(verbosity, "\n");
     verbose(verbosity, "\tpredictor:");
     for (ref = set->predictorList; ref != NULL; ref = ref->next)
         {
	 struct lockedSet *ls = ref->val;
	 if (ls != set)
	     verbose(verbosity, " %s", ls->name);
	 }
     verbose(verbosity, "\n");
     }
}

static struct slName *findPartingDivs(struct fieldedTable *table)
/* Build up list of fields to part table on when doing global parting */
{
struct fieldInfo *fieldList = makeFieldInfo(table);
verbose(3, "made fieldInfo\n");
struct lockedSet *lockedSetList = findLockedSets(table, fieldList);
verbose(3, "%d locked sets\n", slCount(lockedSetList));
dumpLockedSetList(lockedSetList, 3);

/* Make up list of fields to partition on, basically starting with best scoring,
 * and going in order, but not doing ones that are predicted by previous fields */
struct slName *divList = NULL;
struct lockedSet *set;
struct hash *uniqHash = hashNew(0);
for (set = lockedSetList; set != NULL; set = set->next)
    {
    if (!hashLookup(uniqHash, set->name))
         {
	 hashAdd(uniqHash, set->name, NULL);
	 slNameAddHead(&divList, set->name);
	 struct slRef *ref;
	 for (ref = set->predictedList; ref != NULL; ref = ref->next)
	     {
	     struct lockedSet *predSet = ref->val;
	     hashStore(uniqHash, predSet->name);
	     }
	 }
    }
slReverse(&divList);

/* Report our findings */
struct slName *div;
verbose(2, "parting on ");
for (div = divList; div != NULL; div = div->next)
     verbose(2, "%s,", div->name);
verbose(2, "\n");

/* Clean up and return results */
freeHash(&uniqHash);
lockedSetFreeList(&lockedSetList);
fieldInfoFreeList(&fieldList);
return divList;
}

static struct slRef *findPredictableFields(struct fieldedTable *table, struct fieldInfo *fieldList,
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

static boolean findBestParting(struct fieldedTable *table, struct fieldInfo *allFields,
    struct slRef **retFields, struct fieldInfo **retField)
/* Return list of fields that make best partition of table so that it will
 * cause the most information from table to be moved to a higher level.  All the
 * returned fields will move in lock-step. */
{
/* Get locked set list, which is sorted by score, so we only care about first/best */
struct lockedSet *lockedSetList = findLockedSets(table, allFields);
verbose(3, "%d locked sets\n", slCount(lockedSetList));
dumpLockedSetList(lockedSetList, 3);
struct lockedSet *best = lockedSetList;

/* Convert list of lockedSet refs to list of fieldInfo refs */
*retFields = NULL;
struct slRef *setRef;
for (setRef = best->predictedList; setRef != NULL; setRef = setRef->next)
    {
    struct lockedSet *set = setRef->val;
    struct slRef *fieldRef;
    for (fieldRef = set->fieldRefList; fieldRef != NULL; fieldRef = fieldRef->next)
	{
	refAdd(retFields, fieldRef->val);
	}
    }
slReverse(retFields);

/* Also save best field. */
*retField = best->firstField;
lockedSetFreeList(&lockedSetList);
return TRUE;
}

static struct slRef *partOnField(struct fieldedTable *table, struct fieldInfo *allFields,
    char *keyName, struct slName *except, struct fieldInfo **retKeyField)
/* Return list of all fields that covary with keyField */
{
struct fieldInfo *keyField = fieldInfoFind(allFields, keyName);
if (keyField == NULL)
    errAbort("%s not found in fields", keyName);
*retKeyField = keyField;
return findPredictableFields(table, allFields, keyField, except);
}


static boolean doParting(struct fieldedTable *table, struct fieldInfo *allFields,
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

static boolean makeSubtableExcluding(struct fieldedTable *table, 
    int partingFieldIx, char *partingVal,
    struct slRef *excludedFields,
    struct fieldedTable **retSubtable, int ixTranslator[])
/* Make up a subtable that contains all the fields of the table except the excluded ones 
 * Return FALSE if no fields left. */
{
/* Make new subtable with correct fields */
int excludedCount = slCount(excludedFields);
int subFieldCount = table->fieldCount - excludedCount;
if (subFieldCount <= 0)
    return FALSE;
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
return TRUE;
}

static void rPartition(struct fieldedTable *table, struct tagStorm *tagStorm, 
    struct tagStanza *parent, boolean doPrepart, struct slName *prepartField)
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
    verbose(3, "leaf rPartition table of %d cols, %d rows\n", 
	table->fieldCount, table->rowCount);

    // Here is where we should output whole table... 
    struct fieldedRow *row;
    for (row = table->rowList; row != NULL; row = row->next)
        {
	struct tagStanza *stanza = tagStanzaNew(tagStorm, parent);
	int i;
	for (i=0; i<table->fieldCount; ++i)
	    tagStanzaAdd(tagStorm, stanza, table->fields[i], row->row[i]);
	}
    }
else
    {
    int partingFieldIx = partingField->ix;
    verbose(3, "node rPartition table of %d cols, %d rows, fieldName %s, %d locked\n", 
	table->fieldCount, table->rowCount, partingField->name, slCount(partingFields));


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

		/* Advance to next value */
		assert(partValList != NULL);
		partVal = partValList->val;
		partValList = partValList->next;

		/* Make subtable with nonconstant bits starting with header. */
		verbose(3, "parting on %s=%s\n", partingField->name, partVal);
		int ixTranslator[table->fieldCount];
		struct fieldedTable *subtable = NULL;
		if (makeSubtableExcluding(table, partingFieldIx, partVal,
					    partingFields, &subtable, ixTranslator))
		    {
		    rPartition(subtable, tagStorm, stanza, doPrepart, nextPrepart);
		    }
		fieldedTableFree(&subtable);
		}
	    }

	}
    hashFree(&uniq);
    }
slFreeList(&partingFields);
fieldInfoFreeList(&allFields);
}

static struct slPair *removeUnvalued(struct slPair *list)
/* Remove unvalued stanzas (those with "" or "n/a" or "N/A" values */
{
struct slPair *newList = NULL;
struct slPair *pair, *next;
for (pair = list; pair != NULL; pair = next)
    {
    next = pair->next;
    char *val = pair->val;
    if (isDefinedVal(val))
         slAddHead(&newList, pair);
    }
slReverse(&newList);
return newList;
}

static void rRemoveEmptyPairs(struct tagStanza *list)
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->children != NULL)
        rRemoveEmptyPairs(stanza->children);
    stanza->tagList = removeUnvalued(stanza->tagList);
    }
}

static void removeEmptyPairs(struct tagStorm *tagStorm)
/* Remove tags with no values */
{
rRemoveEmptyPairs(tagStorm->forest);
}

static void tagStormFromTab(char *input, char *output)
/* tagStormFromTab - Create a tagStorm file from a tab-separated file where the labels are on the 
 * first line, that starts with a #. */
{
/* Load up input as a fielded table */
struct fieldedTable *table = fieldedTableFromTabFile(input, input, NULL, 0);
verbose(2, "%s has %d fields and %d rows\n", input, table->fieldCount, table->rowCount);

/* Possibly set up global list of partitioning fields */
if (gDivFieldList == NULL && !optionExists("local"))
     {
     gDivFieldList = findPartingDivs(table);
     }

struct tagStorm *tagStorm = tagStormNew(input);
rPartition(table, tagStorm, NULL, gDivFieldList != NULL, gDivFieldList);
tagStormReverseAll(tagStorm);

/* Make output prettier by cleaning up empty pairs and stanzas, hoisting, and ordering
 * fields of stanzas */
verbose(2, "cleaning up empties and hoisting\n");
removeEmptyPairs(tagStorm);
tagStormRemoveEmpties(tagStorm);
if (!optionExists("noHoist"))
    {
    tagStormHoist(tagStorm, NULL);
    }

if (optionExists("keepOrder"))
    tagStormOrderSort(tagStorm, table->fields, table->fieldCount);
else
    tagStormAlphaSort(tagStorm);

/* Write out result */
verbose(2, "writing %s\n", output);
tagStormWrite(tagStorm, output, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

/* Process command line option to drive the partitioning manually. */
char *div = optionVal("div", NULL);
if (div != NULL)
    gDivFieldList = slNameListFromComma(div);

tagStormFromTab(argv[1], argv[2]);
return 0;
}
