/* annoFilter -- autoSql-driven data filtering for annoGratorQuery framework */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoFilter.h"
#include "sqlNum.h"

static struct annoFilter *afFromAsColumnAndIx(struct asColumn *def, int ix,
					      enum annoFilterOp op, void *values)
/* Return a filter for def. */
{
struct annoFilter *newF;
AllocVar(newF);
newF->columnIx = ix;
newF->label = cloneString(def->name);
newF->type = def->lowType->type;
newF->isList = def->isList;
newF->op = op;
newF->values = values;
return newF;
}

static void *valuesFromSlName(struct asColumn *def, struct slName *valList)
/* Parse valList according to def->type. */
{
if (valList != NULL)
    {
    if (asTypesIsFloating(def->lowType->type))
	{
	double *minMax = needMem(2*sizeof(double));
	minMax[0] = atof(valList->name);
	if (valList->next)
	    minMax[1] = atof(valList->next->name);
	return minMax;
	}
    else if (asTypesIsInt(def->lowType->type))
	{
	long long *minMax = needMem(2*sizeof(long long));
	minMax[0] = atoll(valList->name);
	if (valList->next)
	    minMax[1] = atoll(valList->next->name);
	return minMax;
	}
    else
	return slNameCloneList(valList);
    }
return NULL;
}

struct annoFilter *annoFilterFromAsColumn(struct asObject *asObj, char *colName,
					  enum annoFilterOp op, struct slName *valList)
/* Look up description of colName in asObj and return an annoFilter
 * using values parsed from valList if applicable. */
{
struct asColumn *def = asColumnFind(asObj, colName);
if (def == NULL)
    errAbort("annoFilterFromAsColumn: asObject %s column name '%s' not found",
	     asObj->name, colName);
int ix = asColumnFindIx(asObj->columnList, colName);
void *values = valuesFromSlName(def, valList);
return afFromAsColumnAndIx(def, ix, op, values);
}

struct annoFilter *annoFilterFromFunction(AnnoFilterFunction *func, void *values)
/* Return an annoFilter that calls func on each item, using values. */
{
struct annoFilter *newF;
AllocVar(newF);
newF->filterFunc = func;
newF->values = values;
return newF;
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

static boolean matchOneVal(struct slName *wildList, char *val)
/* Return TRUE if val matches at least one wildcard value in wildList. */
{
struct slName *wild;
boolean match = FALSE;
for (wild = wildList;  wild != NULL;  wild = wild->next)
    {
    if (wildMatch(wild->name, val))
	{
	match = TRUE;
	break;
	}
    }
return match;
}

static boolean matchArray(struct slName *wildList, char **rowVals, int valCount)
/* Return TRUE if at least one item in rowVals matches at least one wildcard value in wildList. */
{
int i;
for (i = 0;  i < valCount;  i++)
    if (matchOneVal(wildList, rowVals[i]))
	return TRUE;
return FALSE;
}

static boolean singleFilter(struct annoFilter *filter, char **row, int rowSize)
/* Apply one filter, using either filterFunc or type-based filter on column value.
 * Return TRUE if isExclude and filter passes, or if !isExclude and filter fails. */
{
boolean fail = FALSE;
if (filter->filterFunc != NULL)
    fail = filter->filterFunc(filter, row, rowSize);
else if (filter->op == afMatch || filter->op == afNotMatch)
    {
    struct slName *filterVals = filter->values;
    boolean match;
    if (filter->isList)
	{
	int len = strlen(row[filter->columnIx]);
	char copy[len+1];
	safecpy(copy, len+1, row[filter->columnIx]);
	int valCount = countChars(copy, ',') + 1;
	char *rowVals[valCount];
	valCount = chopCommas(copy, rowVals);
	match = matchArray(filterVals, rowVals, valCount);
	}
    else
	match = matchOneVal(filterVals, row[filter->columnIx]);
    if (filter->op == afMatch)
	fail = !match;
    else
	fail = match;
    }
else
    {
    // column is a number -- integer or floating point?
    enum asTypes type = filter->type;
    if (filter->isList)
	errAbort("annoFilterRowFails: time to implement numeric lists!");
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
if (retRightJoin != NULL)
    *retRightJoin = FALSE;
boolean hasRightJoin = FALSE;
struct annoFilter *filter;
// First pass: left-join filters (failure means ignore this row);
for (filter = filterList;  filter != NULL;  filter = filter->next)
    {
    if (filter->op == afNoFilter || filter->values == NULL || filter->rightJoin)
	{
	hasRightJoin = TRUE;
	continue;
	}
    if (singleFilter(filter, row, rowSize))
	return TRUE;
    }
// Second pass: right-join filters (failure means omit not only this row, but the primary row too)
if (hasRightJoin)
    {
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
