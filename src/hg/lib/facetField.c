
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "facetField.h"

static int facetValCmpUseCountDesc(const void *va, const void *vb)
/* Compare two facetVal so as to sort them based on useCount with most used first */
{
struct facetVal *a = *((struct facetVal **)va);
struct facetVal *b = *((struct facetVal **)vb);
return b->useCount - a->useCount;
}

static struct facetVal *facetValNew(char *val, int useCount)
/* Create new facetVal structure */
{
struct facetVal *facetVal;
AllocVar(facetVal);
facetVal->val = val;
facetVal->useCount = useCount;
return facetVal;
}

static void facetFieldAdd(struct facetField *facetField, char *tagVal)
/* Add information about tag to facetField */
{
facetField->useCount += 1;
struct facetVal *facetVal = hashFindVal(facetField->valHash, tagVal);
if (facetVal == NULL)
    {
    AllocVar(facetVal);
    hashAddSaveName(facetField->valHash, tagVal, facetVal, &facetVal->val);
    slAddHead(&facetField->valList, facetVal);
    }
facetVal->useCount += 1;
}

static struct facetField *facetFieldNew(char *fieldName)
/* Create a new facetField structure */
{
struct facetField *facetField;
AllocVar(facetField);
facetField->fieldName = cloneString(fieldName);
facetField->valHash = hashNew(0);
return facetField;
}

struct facetField *facetFieldsFromSqlTable(struct sqlConnection *conn, char *table, char *fields[], int fieldCount, 
    char *nullVal, char *where)
/* Return a list of facetField, one for each field of given table */
{
/* Make query string */
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select %s", fields[0]);
int i;
for (i=1; i<fieldCount; ++i)
    {
    dyStringAppendC(query, ',');
    dyStringAppend(query, fields[i]);
    }
dyStringPrintf(query, " from %s", table);
if (where != NULL)
    dyStringPrintf(query, " where %s", where);

/* Create facetField list and table. */
struct facetField *ffArray[fieldCount], *ffList = NULL, *ff;
for (i=0; i<fieldCount; ++i)
    {
    ff = ffArray[i] = facetFieldNew(fields[i]);
    slAddHead(&ffList, ff);
    }
slReverse(&ffList);

/* Scan through result saving it in list. */
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    for (i=0; i<fieldCount; ++i)
	{
	char *val = row[i];
	if (val == NULL)
	    val = nullVal;
	if (val != NULL)
	    facetFieldAdd(ffArray[i], val);
	}
    }

/* Sort lists */
for (ff = ffList; ff != NULL; ff = ff->next)
    slSort(&ff->valList, facetValCmpUseCountDesc);

/* Clean up and go home */
dyStringFree(&query);
sqlFreeResult(&sr);
return ffList;
}

struct facetVal *facetValMajorPlusOther(struct facetVal *list, double minRatio)
/* Return a list of only the tags that are over minRatio of total tags.
 * If there are tags that have smaller amounts than this, lump them together
 * under "other".  Returns a new list. Use slFreeList() to free the result.  */
{
/* Figure out total and minimum count to not be lumped into other */
long total = 0;
struct facetVal *tc;
for (tc = list; tc != NULL; tc = tc->next)
    total += tc->useCount;
long minCount = round(minRatio * total);

/* Loop through and copy ones over threshold to new list, and lump rest
 * into other */
struct facetVal *newList = NULL, *newTc;
struct facetVal *other = NULL;
int otherCount = 0;
for (tc = list; tc != NULL; tc = tc->next)
    {
    if (tc->useCount >= minCount)
        {
	newTc = facetValNew(tc->val, tc->useCount);
	slAddHead(&newList, newTc);
	}
    else
        {
	if (other == NULL)
	    other = facetValNew("", 0);
	other->useCount += tc->useCount;
	++otherCount;
	}
    }

/* Put other onto new list if it exists */
if (other != NULL)
    {
    char label[64];
    safef(label, sizeof(label), "%d other", otherCount);
    other->val = cloneString(label);
    slAddHead(&newList, other);
    }

/* Restore reversed order and return result */
slReverse(&newList);
return newList;
}

