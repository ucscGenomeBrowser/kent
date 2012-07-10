/* annoFilter -- autoSql-driven data filtering for annoGratorQuery framework */

#include "annoFilter.h"
#include "sqlNum.h"

struct annoFilter *annoFiltersFromAsObject(struct asObject *asObj)
/* Translate a table's autoSql representation into a list of annoFilters that make
 * sense for the table's set of fields. */
{
struct annoFilter *filterList = NULL;
struct asColumn *def;
int ix;
for (ix = 0, def = asObj->columnList;  def != NULL;  ix++, def = def->next)
    {
    struct annoFilter *newF;
    AllocVar(newF);
    newF->columnIx = ix;
    newF->label = cloneString(def->name);
    newF->type = def->lowType->type;
    newF->op = afNoFilter;
    slAddHead(&filterList, newF);
    }
slReverse(&filterList);
return filterList;
}

static void *cloneValues(void *valuesIn, enum asTypes type)
/* If valuesIn is non-null, return a copy of values according to type. */
{
void *valuesOut = NULL;
if (valuesIn != NULL)
    {
    if (asTypesIsFloating(type))
	valuesOut = cloneMem(valuesIn, 2*sizeof(double));
    else if (asTypesIsInt(type))
	valuesOut = cloneMem(valuesIn, 2*sizeof(long long));
    else
	valuesOut = cloneString((char *)valuesIn);
    }
return valuesOut;
}

struct annoFilter *annoFilterCloneList(struct annoFilter *list)
/* Copy a list of annoFilters. */
{
struct annoFilter *newList = NULL, *oldF;
for (oldF = list;  oldF != NULL;  oldF = oldF->next)
    {
    struct annoFilter *newF = CloneVar(oldF);
    newF->label = cloneString(oldF->label);
    newF->values = cloneValues(oldF->values, oldF->type);
    slAddHead(&newList, newF);
    }
slReverse(&newList);
return newList;
}

void annoFilterFreeList(struct annoFilter **pList)
/* Free a list of annoFilters. */
{
if (pList == NULL)
    return;
struct annoFilter *filter, *nextFilter;
for (filter = *pList;  filter != NULL;  filter = nextFilter)
    {
    nextFilter = filter->next;
    freeMem(filter->label);
    freeMem(filter->values);
    freeMem(filter);
    }
*pList = NULL;
}

static boolean annoFilterDouble(struct annoFilter *filter, double val)
/* Compare val to double(s) in filter->values according to filter->op,
 * return TRUE if comparison fails. */
{
double *filterVals = filter->values;
if (filter->op == afInRange)
    {
    double min = filterVals[0], max = filterVals[1];
    return (val < min || val > max);
    }
else
    {
    double threshold = filterVals[0];
    switch (filter->op)
	{
	case afLT:
	    return !(val < threshold);
	case afLTE:
	    return !(val <= threshold);
	case afEqual:
	    return !(val == threshold);
	case afNotEqual:
	    return !(val != threshold);
	case afGTE:
	    return !(val >= threshold);
	case afGT:
	    return !(val > threshold);
	default:
	    errAbort("annoFilterDouble: unexpected filter->op %d for %s",
		     filter->op, filter->label);
	}
    }
return FALSE;
}

static boolean annoFilterLongLong(struct annoFilter *filter, long long val)
/* Compare val to long long(s) in filter->values according to filter->op,
 * return TRUE if comparison fails. */
{
long long *filterVals = filter->values;
if (filter->op == afInRange)
    {
    long long min = filterVals[0], max = filterVals[1];
    return (val < min || val > max);
    }
else
    {
    long long threshold = filterVals[0];
    switch (filter->op)
	{
	case afLT:
	    return !(val < threshold);
	case afLTE:
	    return !(val <= threshold);
	case afEqual:
	    return !(val == threshold);
	case afNotEqual:
	    return !(val != threshold);
	case afGTE:
	    return !(val >= threshold);
	case afGT:
	    return !(val > threshold);
	default:
	    errAbort("annoFilterLongLong: unexpected filter->op %d for %s",
		     filter->op, filter->label);
	}
    }
return FALSE;
}

static boolean singleFilter(struct annoFilter *filter, char **row, int rowSize)
/* Apply one filter, using either filterFunc or type-based filter on column value.
 * Return TRUE if isExclude and filter passes, or if !isExclude and filter fails. */
{
boolean fail = FALSE;
if (filter->filterFunc != NULL)
    fail = filter->filterFunc(filter, row, rowSize);
else if (filter->op == afMatch)
    fail = !wildMatch((char *)(filter->values), row[filter->columnIx]);
else if (filter->op == afNotMatch)
    fail = wildMatch((char *)(filter->values), row[filter->columnIx]);
else
    {
    // column is a number -- integer or floating point?
    enum asTypes type = filter->type;
    if (asTypesIsFloating(type))
	fail = annoFilterDouble(filter, sqlDouble(row[filter->columnIx]));
    else if (asTypesIsInt(type))
	fail = annoFilterLongLong(filter, sqlLongLong(row[filter->columnIx]));
    else
	errAbort("annoFilterRowFails: unexpected enum asTypes %d for numeric filter op %d",
		 type, filter->op);
    }
if ((filter->isExclude && !fail) || (!filter->isExclude && fail))
    return TRUE;
return FALSE;
}

boolean annoFilterRowFails(struct annoFilter *filterList, char **row, int rowSize,
			   boolean *retRightJoin)
/* Apply filters to row, using autoSql column definitions to interpret
 * each word of row.  Return TRUE if any filter fails (or passes, if isExclude).
 * Set retRightJoin to TRUE if a rightJoin filter has failed. */
{
if (filterList != NULL && slCount(filterList) != rowSize)
    errAbort("annoFilterRowFails: filterList length %d doesn't match rowSize %d",
	     slCount(filterList), rowSize);
if (retRightJoin != NULL)
    *retRightJoin = FALSE;
struct annoFilter *filter;
// First pass: left-join filters (failure means omit this row from output);
for (filter = filterList;  filter != NULL;  filter = filter->next)
    {
    if (filter->op == afNoFilter || filter->values == NULL || filter->rightJoin)
	continue;
    if (singleFilter(filter, row, rowSize))
	return TRUE;
    }
// Second pass: right-join filters (failure means omit not only this row, but the primary row too)
for (filter = filterList;  filter != NULL;  filter = filter->next)
    {
    if (filter->op == afNoFilter || filter->values == NULL || !filter->rightJoin)
	continue;
    if (singleFilter(filter, row, rowSize))
	{
	if (retRightJoin != NULL)
	    *retRightJoin = TRUE;
	return TRUE;
	}
    }
return FALSE;
}

boolean annoFilterWigValueFails(struct annoFilter *filterList, double value,
				boolean *retRightJoin)
/* Apply filters to value.  Return TRUE if any filter fails (or passes, if isExclude).
 * Set retRightJoin to TRUE if a rightJoin filter has failed. */
{
if (retRightJoin != NULL)
    *retRightJoin = FALSE;
struct annoFilter *filter;
// First pass: left-join filters (failure means omit this row from output);
for (filter = filterList; filter != NULL; filter = filter->next)
    {
    if (filter->op == afNoFilter || filter->rightJoin)
	continue;
    boolean fail = annoFilterDouble(filter, value);
    if ((filter->isExclude && !fail) || (!filter->isExclude && fail))
	return TRUE;
    }
// Second pass: right-join filters (failure means omit not only this row, but the primary row too)
for (filter = filterList; filter != NULL; filter = filter->next)
    {
    if (filter->op == afNoFilter || !filter->rightJoin)
	continue;
    boolean fail = annoFilterDouble(filter, value);
    if ((filter->isExclude && !fail) || (!filter->isExclude && fail))
	{
	if (retRightJoin != NULL)
	    *retRightJoin = TRUE;
	return TRUE;
	}
    }
return FALSE;
}

enum annoFilterOp afOpFromString(char *string)
/* Translate string (e.g. "afNotEqual") into enum value (e.g. afNotEqual). */
{
if (sameString(string, "afNoFilter"))
    return afNoFilter;
else if (sameString(string, "afMatch"))
    return afMatch;
else if (sameString(string, "afNotMatch"))
    return afNotMatch;
else if (sameString(string, "afLT"))
    return afLT;
else if (sameString(string, "afLTE"))
    return afLTE;
else if (sameString(string, "afEqual"))
    return afEqual;
else if (sameString(string, "afNotEqual"))
    return afNotEqual;
else if (sameString(string, "afGTE"))
    return afGTE;
else if (sameString(string, "afGT"))
    return afGT;
else if (sameString(string, "afInRange"))
    return afInRange;
else
    errAbort("afOpFromString: Can't translate \"%s\" into enum annoFilterOp", string);
// happy compiler, never get here:
return afNoFilter;
}

char *stringFromAfOp(enum annoFilterOp op)
/* Translate op into a string.  Do not free result. */
{
char *str = "afNoFilter";
switch (op)
    {
    case afNoFilter:
	break;
    case afMatch:
	str = "afMatch";
    case afNotMatch:
	str = "afNotMatch";
	break;
    case afLT:
	str = "afLT";
	break;
    case afLTE:
	str = "afLTE";
	break;
    case afEqual:
	str = "afEqual";
	break;
    case afNotEqual:
	str = "afNotEqual";
	break;
    case afGTE:
	str = "afGTE";
	break;
    case afGT:
	str = "afGT";
	break;
    case afInRange:
	str = "afInRange";
	break;
    default:
	errAbort("stringFromAfOp: unrecognized enum annoFilterOp %d", op);
    }
return str;
}
