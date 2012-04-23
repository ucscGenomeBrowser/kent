/* annoFilter -- autoSql-driven data filtering for annoGratorQuery framework */

#include "annoFilter.h"
#include "sqlNum.h"

struct annoFilter *annoFiltersFromAsObject(struct asObject *asObj)
/* Translate a table's autoSql representation into a list of annoFilters that make
 * sense for the table's set of fields.
 * Callers: do not modify any filter's columnDef! */
{
struct annoFilter *filterList = NULL;
struct asColumn *def;
for (def = asObj->columnList;  def != NULL;  def = def->next)
    {
    struct annoFilter *newF;
    AllocVar(newF);
    newF->columnDef = def;
    newF->op = afNoFilter;
    }
slReverse(&filterList);
return filterList;
}

struct annoFilter *annoFilterCloneList(struct annoFilter *list)
/* Shallow-copy a list of annoFilters.  Callers: do not modify any filter's columnDef! */
{
struct annoFilter *newList = NULL, *oldF;
for (oldF = list;  oldF != NULL;  oldF = oldF->next)
    {
    struct annoFilter *newF = CloneVar(oldF);
    slAddHead(&newList, newF);
    }
slReverse(&newList);
return newList;
}

void annoFilterFreeList(struct annoFilter **pList)
/* Shallow-free a list of annoFilters. */
{
slFreeList(pList);
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
	    errAbort("annoFilterDouble: unexpected filter->op %d for column %s",
		     filter->op, filter->columnDef->name);
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
	    errAbort("annoFilterLongLong: unexpected filter->op %d for column %s",
		     filter->op, filter->columnDef->name);
	}
    }
return FALSE;
}

static boolean singleFilter(struct annoFilter *filter, char *word)
/* Apply filter to word, using autoSql column definitions to interpret word.
 * Return TRUE if isExclude and filter passes, or if !isExclude and filter fails. */
{
boolean fail = FALSE;
if (filter->op == afMatch)
    fail = !wildMatch((char *)(filter->values), word);
else if (filter->op == afNotMatch)
    fail = wildMatch((char *)(filter->values), word);
else
    {
    // word is a number -- integer or floating point?
    enum asTypes type = filter->columnDef->lowType->type;
    if (type == t_double || t_float)
	fail = annoFilterDouble(filter, sqlDouble(word));
    else if (type == t_char || type == t_int || type == t_uint || type == t_short ||
	     type == t_ushort || type == t_byte || type == t_ubyte ||
	     type == t_off)
	fail = annoFilterLongLong(filter, sqlLongLong(word));
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
int i;
// First pass: left-join filters (failure means omit this row from output);
for (i = 0, filter = filterList;  i < rowSize && filter != NULL;  i++, filter = filter->next)
    {
    if (filter->op == afNoFilter || filter->rightJoin)
	continue;
    if (singleFilter(filter, row[i]))
	return TRUE;
    }
// Second pass: right-join filters (failure means omit not only this row, but the primary row too)
for (i = 0, filter = filterList;  i < rowSize && filter != NULL;  i++, filter = filter->next)
    {
    if (filter->op == afNoFilter || !filter->rightJoin)
	continue;
    if (singleFilter(filter, row[i]))
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
