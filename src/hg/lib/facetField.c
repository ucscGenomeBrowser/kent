
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "facetField.h"
#include "csv.h"

int facetValCmpSelectCountDesc(const void *va, const void *vb)
/* Compare two facetVal so as to sort them based on selectCount with most used first.
 * If two have same count, sort alphabetically on facet val.  */
{
struct facetVal *a = *((struct facetVal **)va);
struct facetVal *b = *((struct facetVal **)vb);
int result = b->selectCount - a->selectCount;
if (result == 0)
    {
    result = strcmp(a->val, b->val);
    }
return result;
}

static int facetValCmpUseCountDesc(const void *va, const void *vb)
/* Compare two facetVal so as to sort them based on useCount with most used first */
{
struct facetVal *a = *((struct facetVal **)va);
struct facetVal *b = *((struct facetVal **)vb);
return b->useCount - a->useCount;
}

int facetValCmp(const void *va, const void *vb)
/* Compare two facetVal alphabetically by val */
{
struct facetVal *a = *((struct facetVal **)va);
struct facetVal *b = *((struct facetVal **)vb);
return strcasecmp(a->val, b->val);
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

static void facetFieldAdd(struct facetField *facetField, char *tagVal, boolean selecting)
/* Add information about tag to facetField */
{
if (!selecting)
    facetField->useCount += 1;
struct facetVal *facetVal = hashFindVal(facetField->valHash, tagVal);
if (facetVal == NULL)
    {
    AllocVar(facetVal);
    hashAddSaveName(facetField->valHash, tagVal, facetVal, &facetVal->val);
    slAddHead(&facetField->valList, facetVal);
    if (selecting)
	{
	facetVal->selected = TRUE;
	facetField->allSelected = FALSE;
	}
    if (facetField->allSelected)
	facetVal->selected = TRUE;
    }
if (!selecting)
    facetVal->useCount += 1;
facetField->currentVal = facetVal;
}

static struct facetField *facetFieldNew(char *fieldName)
/* Create a new facetField structure */
{
struct facetField *facetField;
AllocVar(facetField);
facetField->fieldName = cloneString(fieldName);
facetField->valHash = hashNew(0);
facetField->allSelected = TRUE;  // default to all values selected
return facetField;
}

boolean perRowFacetFields(int fieldCount, char **row, char *nullVal, struct facetField *ffArray[])
/* Process each row of a resultset updating use and select counts. 
 * Returns TRUE if row passes selected facet values filter and should be included in the final result set. */
{
boolean result = FALSE;
int totalSelectedFacets = 0;
int facetCount = 0;  // non-NULL facet count
int i;
for (i=0; i<fieldCount; ++i)
    {
    char *val = row[i];
    if (val == NULL)
	val = nullVal;
    if (val != NULL)
	{
	facetFieldAdd(ffArray[i], val, FALSE);
	if (ffArray[i]->currentVal->selected)
	    ++totalSelectedFacets;
	++facetCount;
	}
    else
	{
	ffArray[i]->currentVal = NULL;
	}
    }

if ((totalSelectedFacets == facetCount) && (facetCount > 0))
    {
    // include this row in the final resultset
    result = TRUE;
    }
for (i=0; i<fieldCount; ++i)
    {
    if (ffArray[i]->currentVal) // disregard null values
	{
	// disregard one's self.
	if ((totalSelectedFacets - (int)ffArray[i]->currentVal->selected) == (facetCount - 1))
	    ffArray[i]->currentVal->selectCount++;
	    // shown on GUI to guide choosing by user
	}
    }
return result;
}

#define facetValStringPunc '~'

char *linearizeFacetVals(struct facetField *selectedList)
/* Linearize selected fields vals into a string. fieldVal must be selected. */
{
struct dyString *dy = newDyString(1024);
struct facetField *sff = NULL;
for (sff = selectedList; sff; sff=sff->next)
    {
    dyStringPrintf(dy, "%s %d ", sff->fieldName, sff->showAllValues);
    boolean first = TRUE;
    struct facetVal *el;
    for (el=sff->valList; el; el=el->next)
	{
	if (el->selected)
	    {
	    if (first)
		{
		first = FALSE;
		}
	    else
		dyStringAppendC(dy, ',');
	    // TODO encode as csv val to support doubled internal quote value?
	    dyStringPrintf(dy, "\"%s\"", el->val);  
	    }
	}
    dyStringAppendC(dy, facetValStringPunc);
    }
return dyStringCannibalize(&dy);
}

void selectedListFacetValUpdate(struct facetField **pSelectedList, char *facetName, char *facetValue, char *op)
/* Add or remove by changing selected boolean
 * Reset entire facet by setting all vals to selected = false for the facet */
{
struct facetField *selectedList = *pSelectedList;
struct facetField *sff = NULL;
if (sameString(op, "resetAll"))  // remove all selections
    {
    *pSelectedList = NULL;
    return;
    }

for (sff = selectedList; sff; sff=sff->next)
    {
    if (sameString(sff->fieldName, facetName))
	{
	break;
	}
    }
if (!sff)
    {
    sff = facetFieldNew(facetName);
    slAddHead(&selectedList, sff);
    }

if (sameString(op, "add"))
    {
    facetFieldAdd(sff, facetValue, TRUE);
    sff->currentVal->selected = TRUE;
    }
else if (sameString(op, "remove"))
    {
    facetFieldAdd(sff, facetValue, TRUE);
    sff->currentVal->selected = FALSE;
    }
else if (sameString(op, "showAllValues"))  // show all facet values
    {
    sff->showAllValues = TRUE;
    }
else if (sameString(op, "showSomeValues"))  // show all facet values
    {
    sff->showAllValues = FALSE;
    }
else if (sameString(op, "reset"))
    {
    sff->showAllValues = FALSE;
    struct facetVal *el;
    for (el=sff->valList; el; el=el->next)
	{
	el->selected = FALSE;
	}
    }
*pSelectedList = selectedList;
}

struct facetField *deLinearizeFacetValString(char *selectedFields)
/* Turn linearized selected fields string back into facet structures */
{
struct facetField *ffList = NULL, *ff;
boolean done = FALSE;
while (!done)
    {
    if (sameString(selectedFields, ""))
	break;
    char *end = strchr(selectedFields, facetValStringPunc);
    if (!end)
	{
	end = selectedFields + strlen(selectedFields);
	done = TRUE;
	}
    char saveEnd = *end;
    *end = 0;
    char *spc = strchr(selectedFields, ' ');
    if (!spc)
	errAbort("expected space separating fieldname from showAllValues boolean");
    *spc = 0;

    char *showAllValuesBoolean = spc+1;
    char *spc2 = strchr(showAllValuesBoolean, ' ');
    if (!spc2)
	errAbort("expected space separating showAllValues boolean from facet values list");
    *spc2 = 0;

    char *vals = spc2+1;
    struct slName *valList = NULL;
    if (*vals != 0)  // we have something
	valList = csvParse(vals);

    ff = facetFieldNew(selectedFields);
    ff->showAllValues = (*showAllValuesBoolean == '1');

    slAddHead(&ffList, ff);
    struct slName *el;
    for (el=valList; el; el=el->next)
	{
	//uglyf("adding selected field %s val %s\n", selectedFields, el->name);
	facetFieldAdd(ff, el->name, TRUE);
	}
    
    *spc  = ' ';  // restore
    *spc2 = ' ';  // restore
    *end = saveEnd;
    selectedFields = end+1;
    }
slReverse(&ffList);
return ffList;
}

struct facetField *facetFieldsFromSqlTableInit(char *fields[], int fieldCount, char *selectedFields, struct facetField *ffArray[])
/* Initialize ffList and ffArray and selected facet values */
{
/* Create facetField list and table. */
struct facetField *ffList = NULL, *ff;
int i;
for (i=0; i<fieldCount; ++i)
    {
    ff = ffArray[i] = facetFieldNew(fields[i]);
    slAddHead(&ffList, ff);
    }
slReverse(&ffList);

/* Initialize selected facet values */
if (selectedFields)
    {
    //warn("facetFieldsFromSqlTableInit: selectedFields is NOT NULL [%s]\n", selectedFields);  // TODO REMOVE DEBUG
    struct facetField *selectedList = deLinearizeFacetValString(selectedFields);
    struct facetField *sff = NULL;
    for (sff = selectedList; sff; sff=sff->next)
	{
	//warn("facetFieldsFromSqlTableInit: looking for ff->fieldName [%s]\n", sff->fieldName);  // TODO REMOVE DEBUG
	for (i=0; i<fieldCount; ++i)
	    {
	    if (sameString(ffArray[i]->fieldName, sff->fieldName))
		{
	        //warn("facetFieldsFromSqlTableInit: found ffArray[%d]->fieldName [%s]\n", i, ffArray[i]->fieldName);  // TODO REMOVE DEBUG
		ffArray[i]->showAllValues = sff->showAllValues;
		struct facetVal *el;
		for (el=sff->valList; el; el=el->next)
		    {
		    //warn("adding selected field %s val %s\n", sff->fieldName, el->val);  // TODO REMOVE DEBUG
		    facetFieldAdd(ffArray[i], el->val, TRUE);
		    }
		}
	    }
        
	}
    }
return ffList;
}

void facetFieldsFromSqlTableFinish(struct facetField *ffList, int (*compare )(const void *elem1,  const void *elem2))
/* Do final cleanup after passing over rows */
{
/* Sort lists */
struct facetField *ff;
for (ff = ffList; ff != NULL; ff = ff->next)
    slSort(&ff->valList, compare);
}


struct facetField *facetFieldsFromSqlTable(struct sqlConnection *conn, char *table, char *fields[], int fieldCount, 
    char *nullVal, char *where, char *selectedFields, int *pSelectedRowCount)
/* Return a list of facetField, one for each field of given table */
{

/* Make query string */
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select %s", fields[0]);
int i;
for (i=1; i<fieldCount; ++i)
    {
    sqlDyStringPrintf(query, ",");
    sqlDyStringPrintf(query, "%s", fields[i]);
    }
sqlDyStringPrintf(query, " from %s", table);
if (where != NULL)
    sqlDyStringPrintf(query, " where %-s", where); // trusting where-clause


struct facetField *ffArray[fieldCount];
struct facetField *ffList = facetFieldsFromSqlTableInit(fields, fieldCount, selectedFields, ffArray);

int selectedRowCount = 0;

/* Scan through result saving it in list. */
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (perRowFacetFields(fieldCount, row, nullVal, ffArray))
	++selectedRowCount;
    }

facetFieldsFromSqlTableFinish(ffList, facetValCmpUseCountDesc);

/* Clean up and go home */
dyStringFree(&query);
sqlFreeResult(&sr);

if (pSelectedRowCount)
    *pSelectedRowCount = selectedRowCount;
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

